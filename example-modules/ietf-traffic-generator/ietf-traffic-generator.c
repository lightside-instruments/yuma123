/*
    module ietf-traffic-generator
    implementation for Linux
    namespace urn:ietf:params:xml:ns:yang:ietf-traffic-generator
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

#include "procdefs.h"
#include "agt.h"
#include "agt_cb.h"
#include "agt_commit_complete.h"
#include "agt_timer.h"
#include "agt_util.h"
#include "agt_not.h"
#include "agt_rpc.h"
#include "dlq.h"
#include "ncx.h"
#include "ncxmod.h"
#include "ncxtypes.h"
#include "status.h"
#include "rpc.h"
#include "val.h"
#include "val123.h"

#define IF_MOD "ietf-interfaces"
#define TG_MOD "ietf-traffic-generator"

/* module static variables */
static ncx_module_t *ietf_interfaces_mod;
static ncx_module_t *iana_if_type_mod;
static ncx_module_t *ietf_traffic_generator_mod;

static status_t
    get_traffic_generator_statistics(ses_cb_t *scb,
                         getcb_mode_t cbmode,
                         val_value_t *vir_val,
                         val_value_t *dst_val);


/*
    {"interface-name", required_argument, NULL, 'i'},
    {"frame-size", required_argument, NULL, 's'},
    {"frame-data", required_argument, NULL, 'd'},
    {"interframe-gap", required_argument, NULL, 'f'},
    {"interburst-gap", required_argument, NULL, 'b'},
    {"frames-per-burst", required_argument, NULL, 'n'},
    {"bursts-per-stream", required_argument, NULL, 'p'},
    {"total-frames", required_argument, NULL, 't'},
    {"testframe-type", required_argument, NULL, 'T'},
*/

static void serialize_params_stream(val_value_t* parent /*either traffic-generator or stream */, char* cli_args_str, char* id)
{
    val_value_t* val;

    val = val_find_child(parent,"ietf-traffic-generator","frame-size");
    sprintf(cli_args_str+strlen(cli_args_str)," --frame-size%s=%u",id,VAL_UINT32(val));

    val = val_find_child(parent,"ietf-traffic-generator","frame-data");
    if(val!=NULL) {
        sprintf(cli_args_str+strlen(cli_args_str)," --frame-data%s=",id);
        
#if 0
        for(i=0;i<val->v.binary.ustrlen;i++) {
            sprintf(cli_args_str+strlen(cli_args_str),"%02X",(unsigned int)(val->v.binary.ustr[i]));
        }
#else
        sprintf(cli_args_str+strlen(cli_args_str),"%s",VAL_STRING(val));
#endif
    }

    val = val_find_child(parent,"ietf-traffic-generator","interframe-gap");
    sprintf(cli_args_str+strlen(cli_args_str)," --interframe-gap%s=%u",id,VAL_UINT32(val));

    val = val_find_child(parent,"ietf-traffic-generator","interburst-gap");
    if(val!=NULL) {
        sprintf(cli_args_str+strlen(cli_args_str)," --interburst-gap%s=%u",id,VAL_UINT32(val));
    }

    val = val_find_child(parent,"ietf-traffic-generator","frames-per-burst");
    if(val!=NULL) {
        sprintf(cli_args_str+strlen(cli_args_str)," --frames-per-burst%s=%u",id,VAL_UINT32(val));
    }

    val = val_find_child(parent,"ietf-traffic-generator","bursts-per-stream");
    if(val!=NULL) {
        sprintf(cli_args_str+strlen(cli_args_str)," --bursts-per-stream%s=%u",id,VAL_UINT32(val));
    }
    val = val_find_child(parent,"ietf-traffic-generator","interstream-gap");
    if(val!=NULL) {
        sprintf(cli_args_str+strlen(cli_args_str)," --interstream-gap%s=%u",id,VAL_UINT32(val));
    }
}

static void serialize_params(val_value_t* traffic_generator_val, char* cli_args_str)
{
    val_value_t* val;
    val_value_t* streams_val;
    val_value_t* stream_val;
    val_value_t* id_val;

    val = val_find_child(traffic_generator_val->parent,"ietf-interfaces","name");
    sprintf(cli_args_str,"--interface-name=%s",VAL_STRING(val));

    val = val_find_child(traffic_generator_val,"ietf-traffic-generator","testframe-type");
    if(val!=NULL) {
        sprintf(cli_args_str+strlen(cli_args_str)," --testframe-type=%s", val->v.idref.name);
    }

    val = val_find_child(traffic_generator_val,"ietf-traffic-generator","total-frames");
    if(val!=NULL) {
        sprintf(cli_args_str+strlen(cli_args_str)," --total-frames=%lu",VAL_UINT64(val));
    }

    val = val_find_child(traffic_generator_val,"ietf-traffic-generator","realtime-epoch");
    if(val!=NULL) {
        sprintf(cli_args_str+strlen(cli_args_str)," --realtime-epoch=%s",VAL_STRING(val));
    }

    if(streams_val==NULL) {
        serialize_params_stream(stream_val, cli_args_str, "");
    } else {
        for (stream_val = val_get_first_child(streams_val);
             stream_val != NULL;
             stream_val = val_get_next_child(stream_val)) {
            char id_buf[] = "4294967295";

            id_val = val_find_child(stream_val,"ietf-traffic-generator","id");
            sprintf(id_buf, "%u", VAL_UINT32(id_val));
            serialize_params_stream(stream_val, cli_args_str, id_buf);
        }
    }
}


static void traffic_generator_delete(val_value_t* traffic_generator_val)
{
    static char cmd_buf[50000];
    static char cmd_args_buf[40960];
    val_value_t* name_val;
    val_value_t* streams_val;
    val_value_t* stream_val;
    val_value_t* id_val;

    printf("traffic_generator_delete:\n");
    val_dump_value(traffic_generator_val,NCX_DEF_INDENT);

    serialize_params(traffic_generator_val, cmd_args_buf);

    streams_val = val_find_child(traffic_generator_val,"ietf-traffic-generator","streams");

    if(streams_val==NULL) {
        serialize_params_stream(traffic_generator_val, cmd_args_buf, "");
    } else {
        for (stream_val = val_get_first_child(streams_val);
             stream_val != NULL;
             stream_val = val_get_next_child(stream_val)) {

            char id_buf[] = "4294967295";

            id_val = val_find_child(stream_val,"ietf-traffic-generator","id");
            sprintf(id_buf, "%u", VAL_UINT32(id_val));
            serialize_params_stream(stream_val, cmd_args_buf, id_buf);
        }
    }
    sprintf(cmd_buf, "pkill -f 'traffic-generator %s'", cmd_args_buf);
    log_info(cmd_buf);
    system(cmd_buf);

    sprintf(cmd_buf, "traffic-generator %s --disable &", cmd_args_buf);
    log_info(cmd_buf);
    system(cmd_buf);
}

static void traffic_generator_create(val_value_t* traffic_generator_val)
{
    static char cmd_buf[50000];
    static char cmd_args_buf[40960];
    val_value_t* name_val;
    val_value_t* streams_val;
    val_value_t* stream_val;
    val_value_t* id_val;

    printf("traffic_generator_create:\n");
    val_dump_value(traffic_generator_val,NCX_DEF_INDENT);

    serialize_params(traffic_generator_val, cmd_args_buf);

    streams_val = val_find_child(traffic_generator_val,"ietf-traffic-generator","streams");

    if(streams_val==NULL) {
        serialize_params_stream(traffic_generator_val, cmd_args_buf, "");
    } else {
        for (stream_val = val_get_first_child(streams_val);
             stream_val != NULL;
             stream_val = val_get_next_child(stream_val)) {

            char id_buf[] = "4294967295";

            id_val = val_find_child(stream_val,"ietf-traffic-generator","id");
            assert(id_val);
            sprintf(id_buf, "%u", VAL_UINT32(id_val));

            serialize_params_stream(stream_val, cmd_args_buf+strlen(cmd_args_buf), id_buf);
        }
    }
    sprintf(cmd_buf, "traffic-generator %s &", cmd_args_buf);
    log_info(cmd_buf);
    system(cmd_buf);
}

static int update_config(val_value_t* config_cur_val, val_value_t* config_new_val)
{

    status_t res;

    val_value_t *interfaces_cur_val, *interface_cur_val , *traffic_generator_cur_val;
    val_value_t *interfaces_new_val, *interface_new_val , *traffic_generator_new_val;


    if(config_new_val == NULL) {
        interfaces_new_val = NULL;
    } else {
        interfaces_new_val = val_find_child(config_new_val,
                               IF_MOD,
                               "interfaces");
    }

    if(config_cur_val == NULL) {
        interfaces_cur_val = NULL;
    } else {
        interfaces_cur_val = val_find_child(config_cur_val,
                                       IF_MOD,
                                       "interfaces");
    }

    /* 2 step (delete/add) interface configuration */

    /* 1. deactivation loop - deletes all deleted or modified interface/traffic-generator -s */
    if(interfaces_cur_val!=NULL) {
        for (interface_cur_val = val_get_first_child(interfaces_cur_val);
             interface_cur_val != NULL;
             interface_cur_val = val_get_next_child(interface_cur_val)) {
            traffic_generator_cur_val = val_find_child(interface_cur_val, TG_MOD, "traffic-generator");
            if(traffic_generator_cur_val==NULL) {
                continue;
            }
            traffic_generator_new_val = val123_find_match(config_new_val, traffic_generator_cur_val);
            if(traffic_generator_new_val==NULL || 0!=val_compare_ex(traffic_generator_cur_val,traffic_generator_new_val,TRUE)) {
                traffic_generator_delete(traffic_generator_cur_val);
            }
        }
    }

    /* 2. activation loop - adds all new or modified interface/traffic-generator -s */
    if(interfaces_new_val!=NULL) {
        for (interface_new_val = val_get_first_child(interfaces_new_val);
             interface_new_val != NULL;
             interface_new_val = val_get_next_child(interface_new_val)) {
            traffic_generator_new_val = val_find_child(interface_new_val, TG_MOD, "traffic-generator");
            if(traffic_generator_new_val==NULL) {
                continue;
            }
            traffic_generator_cur_val = val123_find_match(config_cur_val, traffic_generator_new_val);
            if(traffic_generator_cur_val==NULL || 0!=val_compare_ex(traffic_generator_new_val,traffic_generator_cur_val,TRUE)) {
                traffic_generator_create(traffic_generator_new_val);
            }
        }
    }
    return NO_ERR;
}

static val_value_t* prev_root_val = NULL;
static int update_config_wrapper()
{
    cfg_template_t        *runningcfg;
    status_t res;
    runningcfg = cfg_get_config_id(NCX_CFGID_RUNNING);
    assert(runningcfg!=NULL && runningcfg->root!=NULL);
    if(prev_root_val!=NULL) {
        val_value_t* cur_root_val;
        cur_root_val = val_clone_config_data(runningcfg->root, &res);
        if(0==val_compare(cur_root_val,prev_root_val)) {
            /*no change*/
            val_free_value(cur_root_val);
            return 0;
        }
        val_free_value(cur_root_val);
    }
    update_config(prev_root_val, runningcfg->root);

    if(prev_root_val!=NULL) {
        val_free_value(prev_root_val);
    }
    prev_root_val = val_clone_config_data(runningcfg->root, &res);

    return 0;
}

static status_t y_commit_complete(void)
{
    update_config_wrapper();
    return NO_ERR;
}

/* The 3 mandatory callback functions: y_ietf_traffic_generator_init, y_ietf_traffic_generator_init2, y_ietf_traffic_generator_cleanup */

status_t
    y_ietf_traffic_generator_init (
        const xmlChar *modname,
        const xmlChar *revision)
{
    agt_profile_t* agt_profile;
    obj_template_t* flows_obj;
    status_t res;

    agt_profile = agt_get_profile();

    res = ncxmod_load_module(
        "ietf-interfaces",
        NULL,
        &agt_profile->agt_savedevQ,
        &ietf_interfaces_mod);
    if (res != NO_ERR) {
        return res;
    }
    res = ncxmod_load_module(
        "iana-if-type",
        NULL,
        &agt_profile->agt_savedevQ,
        &iana_if_type_mod);
    if (res != NO_ERR) {
        return res;
    }
    res = ncxmod_load_module(
        "ietf-traffic-generator",
        NULL,
        &agt_profile->agt_savedevQ,
        &ietf_traffic_generator_mod);
    if (res != NO_ERR) {
        return res;
    }

    agt_disable_feature ("ietf-traffic-generator", "multi-stream");

    res=agt_commit_complete_register("ietf-traffic-generator" /*SIL id string*/,
                                     y_commit_complete);
    assert(res == NO_ERR);

    return res;
}

status_t y_ietf_traffic_generator_init2(void)
{
    status_t res=NO_ERR;
    cfg_template_t* runningcfg;
    ncx_module_t* mod;
    obj_template_t* interfaces_obj;
    val_value_t* root_val;
    val_value_t* interfaces_val;

    runningcfg = cfg_get_config_id(NCX_CFGID_RUNNING);
    assert(runningcfg && runningcfg->root);
    root_val = runningcfg->root;

    y_commit_complete();

    return res;
}

void y_ietf_traffic_generator_cleanup (void)
{
}

