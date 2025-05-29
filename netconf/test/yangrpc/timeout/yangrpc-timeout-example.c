/* Usage:
   ./yangrpc-multi-session-example --server=myserver.com --port=830 --server-alt=myserver.com --port-alt=10830 --user=myuser --password='mypass' \
                     --private-key=/home/myuser/.ssh/id_rsa \
                     --public-key=/home/myuser/.ssh/id_rsa.pub
*/

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>

#include "ncx.h"
#include "ncxmod.h"
#include "val.h"
#include "yangrpc.h"

static struct option const long_options[] =
{
    {"autoload", optional_argument, NULL, 'a'},
    {"server", required_argument, NULL, 's'},
    {"port", required_argument, NULL, 'p'},
    {"user", required_argument, NULL, 'u'},
    {"password", required_argument, NULL, 'P'},
    {"private-key", required_argument, NULL, 'k'},
    {"public-key", required_argument, NULL, 'K'},
    {"netconfd-pid", required_argument, NULL, 'i'},
    {NULL, 0, NULL, 0}
};

void stop_process(unsigned int pid)
{
    kill(pid, SIGSTOP);
//    kill(pid, SIGKILL);
}

int main(int argc, char* argv[])
{
    int i;
    status_t res;
    yangrpc_cb_ptr_t yangrpc_cb_ptr;
    yangrpc_cb_ptr_t yangrpc_cb_ptr_alt;
    ncx_module_t * ietf_netconf_mod;
    obj_template_t* rpc_obj;
    obj_template_t* input_obj;
    obj_template_t* filter_obj;
    
    val_value_t* request_val;
    val_value_t* reply_val;
    val_value_t* filter_val;
    val_value_t* type_meta_val;
    val_value_t* select_meta_val;

    int optc;

    char* server="127.0.0.1";
    unsigned int port=830;
    char* user=getlogin();
    char* password=NULL;
    char private_key[1024];
    char public_key[1024];
    boolean autoload = TRUE;
    char arg_str_buf[1024];

    unsigned int netconfd_pid;

    sprintf(private_key,"/home/%s/.ssh/id_rsa",user);
    sprintf(public_key,"/home/%s/.ssh/id_rsa.pub",user);

    while ((optc = getopt_long (argc, argv, "a:s:p:u:P:k:K", long_options, NULL)) != -1) {
        switch (optc) {
            case 'a':
                if (optarg != NULL && strcasecmp(optarg, "false") == 0)
                    autoload = FALSE;
                break;
            case 's':
                server=optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'i':
                netconfd_pid=atoi(optarg);
                break;
            case 'u':
                user = optarg;
                break;
            case 'P':
                password = optarg;
                break;
            case 'k':
                strcpy(private_key,optarg);
                break;
            case 'K':
                strcpy(public_key,optarg);
                break;
            default:
                exit (-1);
        }
    }


    res = yangrpc_init(NULL);
    assert(res==NO_ERR);
    sprintf(arg_str_buf, "--timeout=3 %s", autoload ? " --autoload=true" : " --autoload=false");

    res = yangrpc_connect(server /*127.0.0.1*/,
                          port /*830*/,
                          user /*myuser*/,
                          password /* "" */,
                          public_key /* "/home/myuser/.ssh/id_rsa.pub" */,
                          private_key /* "/home/myuser/.ssh/id_rsa" */,
                          arg_str_buf,
                          &yangrpc_cb_ptr);
    assert(res==NO_ERR);


    res = yangrpc_parse_cli(yangrpc_cb_ptr, "xget /", &request_val);
    assert(res==0);


    res = yangrpc_exec(yangrpc_cb_ptr, request_val, &reply_val);
    assert(res==0);
    val_free_value(reply_val);

    //stopping the netconfd process so we get timeout
    printf("Blocking the netconfd server with signal ...\n");
    stop_process(netconfd_pid);

    printf("Waiting for timeout ...\n");

    res = yangrpc_exec(yangrpc_cb_ptr, request_val, &reply_val);
    assert(res!=0);

    printf("Timeout res=%u\n", res);

    val_free_value(request_val);

    yangrpc_close(yangrpc_cb_ptr);

    yangrpc_cleanup();

    return 0;
}
