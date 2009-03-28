/*  FILE: var.c

    User variable utilities

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
23-aug-07    abb      begun


*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifndef _H_procdefs
#include "procdefs.h"
#endif

#ifndef _H_log
#include "log.h"
#endif

#ifndef _H_ncx
#include "ncx.h"
#endif

#ifndef _H_ncxconst
#include "ncxconst.h"
#endif

#ifndef _H_ncxmod
#include "ncxmod.h"
#endif

#ifndef _H_runstack
#include "runstack.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifndef _H_val
#include "val.h"
#endif

#ifndef _H_var
#include "var.h"
#endif


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/
#ifdef DEBUG
#define VAR_DEBUG   1
#endif

/********************************************************************
*								    *
*			     T Y P E S				    *
*								    *
*********************************************************************/


/********************************************************************
*                                                                   *
*                       V A R I A B L E S			    *
*                                                                   *
*********************************************************************/


/********************************************************************
* FUNCTION new_var
* 
* Malloc and fill in a new var struct
*
* INPUTS:
*    name == name of var (not Z-terminated)
*    namelen == length of name
*    val == variable value
*    vartype == specified variable type
*    res == address of status result if returning NULL
*
* OUTPUTS:
*    *res == status
*
* RETURNS:
*    malloced struct with direct 'val' string added
*    to be freed later
*********************************************************************/
static ncx_var_t *
    new_var (const xmlChar *name,
	     uint32 namelen,
	     val_value_t *val,
	     var_type_t vartype,
	     status_t *res)
{
    ncx_var_t  *var;

    var = m__getObj(ncx_var_t);
    if (!var) {
	*res = ERR_INTERNAL_MEM;
	return NULL;
    }

    memset(var, 0x0, sizeof(ncx_var_t));

    var->vartype = vartype;

    var->name = xml_strndup(name, namelen);
    if (!var->name) {
	m__free(var);
	*res = ERR_INTERNAL_MEM;
	return NULL;
    }

    var->val = val;

    *res = NO_ERR;
    return var;
	
} /* new_var */


/********************************************************************
* FUNCTION free_var
* 
* Free a previously malloced user var struct
*
* INPUTS:
*    var == user var struct to free
*********************************************************************/
static void
    free_var (ncx_var_t *var)
{
    if (var->name) {
	m__free(var->name);
    }
    if (var->val) {
	val_free_value(var->val);
    }
    m__free(var);

} /* free_var */


/********************************************************************
* FUNCTION get_que
* 
* Get the correct queue for the ncx_var_t struct
*
* INPUTS:
*    vartype == variable type
*    name == name string (only checks first char
*            does not have to be zero terminated!!
*
* RETURNS:
*   correct que header or NULL if internal error
*********************************************************************/
static dlq_hdr_t *
    get_que (var_type_t vartype,
	     const xmlChar *name)
{
    if (isdigit((int)*name)) {
	return runstack_get_parm_que();
    } else {
	switch (vartype) {
	case VAR_TYP_LOCAL:
	case VAR_TYP_SESSION:   /****/
	    return runstack_get_que(FALSE);
	case VAR_TYP_CONFIG:
	case VAR_TYP_GLOBAL:
	case VAR_TYP_SYSTEM:
	    return runstack_get_que(TRUE);
	default:
	    return NULL;
	}
    }
} /* get_que */


/********************************************************************
* FUNCTION var_insert
* 
* Insert a user var 
* 
* INPUTS:
*   var == var to insert
*    que == queue header to contain the var struct
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    insert_var (ncx_var_t *var,
		dlq_hdr_t *que)
{
    ncx_var_t  *cur;
    int         ret;

    for (cur = (ncx_var_t *)dlq_firstEntry(que);
	 cur != NULL;
	 cur = (ncx_var_t *)dlq_nextEntry(cur)) {

	ret = xml_strcmp(var->name, cur->name);
	if (ret < 0) {
	    dlq_insertAhead(var, cur);
	    return NO_ERR;
	} else if (ret == 0) {
	    return SET_ERROR(ERR_NCX_DUP_ENTRY);
	} /* else keep going */
    }

    /* if we get here, then new first entry */
    dlq_enque(var, que);
    return NO_ERR;

}  /* insert_var */


/********************************************************************
* FUNCTION remove_var
* 
* Remove a user var 
* 
* INPUTS:
*   varQ == que to use (NULL if not known yet)
*   name == var name to remove
*   namelen == length of name
*   nsid == namespace ID to check if non-zero
*   vartype == variable type
*
* RETURNS:
*   found var struct or NULL if not found
*********************************************************************/
static ncx_var_t *
    remove_var (dlq_hdr_t *varQ,
		const xmlChar *name,
		uint32 namelen,
		xmlns_id_t nsid,
		var_type_t vartype)
{
    ncx_var_t  *cur;
    int         ret;

    if (!varQ) {
	varQ = get_que(vartype, name);
	if (!varQ) {
	    SET_ERROR(ERR_INTERNAL_VAL);
	    return NULL;
	}
    }

    for (cur = (ncx_var_t *)dlq_firstEntry(varQ);
	 cur != NULL;
	 cur = (ncx_var_t *)dlq_nextEntry(cur)) {

	if (nsid && cur->nsid && nsid != cur->nsid) {
	    continue;
	}

	ret = xml_strncmp(name, cur->name, namelen);
	if (ret == 0 && xml_strlen(cur->name)==namelen) {
	    dlq_remove(cur);
	    return cur;
	} /* else keep going */
    }

    return NULL;

}  /* remove_var */


/********************************************************************
* FUNCTION find_var
* 
* Find a user var 
* 
* INPUTS:
*   varQ == que to use or NULL if not known
*   name == var name to find
*   namelen == name length
*   nsid == namespace ID to check if non-zero
*   vartype == variable type
*
* RETURNS:
*   found var struct or NULL if not found
*********************************************************************/
static ncx_var_t *
    find_var (dlq_hdr_t *varQ,
	      const xmlChar *name,
	      uint32  namelen,
	      xmlns_id_t  nsid,
	      var_type_t vartype)
{
    ncx_var_t  *cur;
    int         ret;

    if (!varQ) {
	varQ = get_que(vartype, name);
	if (!varQ) {
	    SET_ERROR(ERR_INTERNAL_VAL);
	    return NULL;
	}
    }

    for (cur = (ncx_var_t *)dlq_firstEntry(varQ);
	 cur != NULL;
	 cur = (ncx_var_t *)dlq_nextEntry(cur)) {

	if (nsid && cur->nsid && nsid != cur->nsid) {
	    continue;
	}

	ret = xml_strncmp(name, cur->name, namelen);
	if (ret == 0 && xml_strlen(cur->name)==namelen) {
	    return cur;
	} /* else keep going */
    }

    return NULL;

}  /* find_var */


/********************************************************************
* FUNCTION set_str
* 
* Find and set (or create a new) global user variable
* Common portions only!!!
*
* Force caller to deallocate var if there is an error
*
* INPUTS:
*   varQ == queue to use or NULL if not known yet
*   name == var name to set
*   namelen == length of name
*   val == var value to set
*   vartype == variable type
* 
* RETURNS:
*   status
*********************************************************************/
static status_t
    set_str (dlq_hdr_t *varQ,
	     const xmlChar *name,
	     uint32 namelen,
	     val_value_t *val,
	     var_type_t vartype)
{
    val_value_t  *tempval;
    ncx_var_t    *var;
    status_t      res;

    res = NO_ERR;

    if (!val->name) {
	val_set_name(val, name, namelen);
    }

    /* try to find this var */
    var = find_var(varQ, name, namelen, 0, vartype);
    if (var) {
	if (var->vartype == VAR_TYP_SYSTEM) {
	    log_error("\nError: system variables cannot be changed");
	    return ERR_NCX_VAR_READ_ONLY;
	}

	/* only allow local vars to change the data type */
	if ((vartype != VAR_TYP_LOCAL) &&
	    (val->btyp != var->val->btyp ||
	     val->typdef != var->val->typdef)) {
	    log_error("\nError: cannot change the variable data type");
	    return ERR_NCX_WRONG_TYPE;
	}

	/* swap out the vars, but keep the old QName */
	tempval = var->val;
	var->val = val;
	var->val->nsid = tempval->nsid;
	val_set_name(var->val, tempval->name, 
		     xml_strlen(tempval->name));
	val_free_value(tempval);
    } else {
	if (!varQ) {
	    varQ = get_que(vartype, name);
	    if (!varQ) {
		return SET_ERROR(ERR_INTERNAL_VAL);
	    }
	}

	/* create a new value */
	var = new_var(name, namelen, val, vartype, &res);
	if (!var || res != NO_ERR) {
	    if (var) {
		var->val = NULL;
		free_var(var);
	    }
	    return res;
	}

	res = insert_var(var, varQ);
	if (res != NO_ERR) {
	    var->val = NULL;
	    free_var(var);
	}
    }
    return res;

}  /* set_str */


/*************   E X T E R N A L   F U N C T I O N S   ************/


/********************************************************************
* FUNCTION var_free
* 
* Free a ncx_var_t struct
* 
* INPUTS:
*   var == var struct to free
* 
*********************************************************************/
void
    var_free (ncx_var_t *var)
{
#ifdef DEBUG
    if (!var) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif

    if (var->val) {
	val_free_value(var->val);
    }
    if (var->name) {
	m__free(var->name);
    }
    m__free(var);

}  /* var_free */



/********************************************************************
* FUNCTION var_clean_varQ
* 
* Clean a Q of ncx_var_t
* 
* INPUTS:
*   varQ == Q of var structs to free
* 
*********************************************************************/
void
    var_clean_varQ (dlq_hdr_t *varQ)
{
    ncx_var_t *var;

#ifdef DEBUG
    if (!varQ) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif

    while (!dlq_empty(varQ)) {
	var = (ncx_var_t *)dlq_deque(varQ);
	var_free(var);
    }

}  /* var_clean_varQ */


/********************************************************************
* FUNCTION var_set_str
* 
* Find and set (or create a new) global user variable
* 
* INPUTS:
*   name == var name to set
*   namelen == length of name
*   value == var value to set
*   vartype == variable type
* 
* RETURNS:
*   status
*********************************************************************/
status_t
    var_set_str (const xmlChar *name,
		 uint32 namelen,
		 const val_value_t *value,
		 var_type_t vartype)
{
    val_value_t  *val;
    status_t      res;

#ifdef DEBUG
    if (!name || !value) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
    if (!namelen) {
	return SET_ERROR(ERR_INTERNAL_VAL);
    }
#endif

    if (vartype == VAR_TYP_NONE || vartype > VAR_TYP_SYSTEM) {
	return ERR_NCX_INVALID_VALUE;
    }

    val = val_clone(value);
    if (!val) {
	return ERR_INTERNAL_MEM;
    }

    res = set_str(NULL, name, namelen, val, vartype);
    if (res != NO_ERR) {
	val_free_value(val);
    }
    return res;

}  /* var_set_str */


/********************************************************************
* FUNCTION var_set
* 
* Find and set (or create a new) global user variable
* 
* INPUTS:
*   name == var name to set
*   value == var value to set
*   vartype == variable type
* 
* RETURNS:
*   status
*********************************************************************/
status_t
    var_set (const xmlChar *name,
	     const val_value_t *value,
	     var_type_t vartype)
{
#ifdef DEBUG
    if (!name) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    return var_set_str(name, 
		       xml_strlen(name),
		       value, 
		       vartype);

}  /* var_set */


/********************************************************************
* FUNCTION var_set_str_que
* 
* Find and set (or create a new) global user variable
* 
* INPUTS:
*   varQ == variable binding Q to use instead of runstack
*   name == var name to set
*   namelen == length of name
*   value == var value to set
* 
* RETURNS:
*   status
*********************************************************************/
status_t
    var_set_str_que (dlq_hdr_t *varQ,
		     const xmlChar *name,
		     uint32 namelen,
		     const val_value_t *value)
{
    val_value_t  *val;
    status_t      res;

#ifdef DEBUG
    if (!varQ || !name || !value) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
    if (!namelen) {
	return SET_ERROR(ERR_INTERNAL_VAL);
    }
#endif

    val = val_clone(value);
    if (!val) {
	return ERR_INTERNAL_MEM;
    }

    res = set_str(varQ, name, namelen, val, VAR_TYP_QUEUE);
    if (res != NO_ERR) {
	val_free_value(val);
    }
    return res;

}  /* var_set_str_que */


/********************************************************************
* FUNCTION var_set_que
* 
* Find and set (or create a new) Q-based user variable
* 
* INPUTS:
*   varQ == varbind Q to use
*   name == var name to set
*   value == var value to set
* 
* RETURNS:
*   status
*********************************************************************/
status_t
    var_set_que (dlq_hdr_t *varQ,
		 const xmlChar *name,
		 const val_value_t *value)
{
#ifdef DEBUG
    if (!varQ || !name) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    return var_set_str_que(varQ, name, xml_strlen(name), value);

}  /* var_set_que */


/********************************************************************
* FUNCTION var_set_move
* 
* Find and set (or create a new) global user variable
* Use the provided entry which will be freed later
* This function will not clone the value like var_set
*
* INPUTS:
*   name == var name to set
*   namelen == length of name string
*   vartype == variable type
*   value == var value to set
* 
* RETURNS:
*   status
*********************************************************************/
status_t
    var_set_move (const xmlChar *name,
		  uint32 namelen,
		  var_type_t vartype,
		  val_value_t *value)
{
#ifdef DEBUG
    if (!name || !value) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
    if (!namelen) {
	return SET_ERROR(ERR_INTERNAL_VAL);
    }
#endif

    if (vartype == VAR_TYP_NONE || vartype > VAR_TYP_SYSTEM) {
	return ERR_NCX_INVALID_VALUE;
    }

    return set_str(NULL, name, namelen, value, vartype);

}  /* var_set_move */


/********************************************************************
* FUNCTION var_set_sys
* 
* Find and set (or create a new) global system variable
* 
* INPUTS:
*   name == var name to set
*   value == var value to set
* 
* RETURNS:
*   status
*********************************************************************/
status_t
    var_set_sys (const xmlChar *name,
		 const val_value_t *value)
{
    val_value_t  *val;
    status_t      res;

#ifdef DEBUG
    if (!name || !value) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    val = val_clone(value);
    if (!val) {
	return ERR_INTERNAL_MEM;
    }

    res = set_str(NULL, name, xml_strlen(name), val, VAR_TYP_SYSTEM);
    if (res != NO_ERR) {
	val_free_value(val);
    }
    return res;

}  /* var_set_sys */


/********************************************************************
* FUNCTION var_set_from_string
* 
* Find and set (or create a new) global user variable
* from a string value instead of a val_value_t struct
*
* INPUTS:
*   name == var name to set
*   valstr == value string to set
*   vartype == variable type
* 
* RETURNS:
*   status
*********************************************************************/
status_t
    var_set_from_string (const xmlChar *name,
			 const xmlChar *valstr,
			 var_type_t vartype)
{
    const obj_template_t  *genstr;
    val_value_t           *val;
    status_t               res;

#ifdef DEBUG
    if (!name) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    if (vartype == VAR_TYP_NONE || vartype > VAR_TYP_SYSTEM) {
	return ERR_NCX_INVALID_VALUE;
    }

    genstr = ncx_get_gen_string();
    if (!genstr) {
	return SET_ERROR(ERR_INTERNAL_VAL);
    }

    /* create a value struct to store */
    val = val_new_value();
    if (!val) {
	return ERR_INTERNAL_MEM;
    }

    val_init_from_template(val, genstr);

    /* create a string value */
    res = val_set_string(val, name, valstr);
    if (res != NO_ERR) {
	val_free_value(val);
	return res;
    }

    /* change the name of the value to the variable node
     * instead of the generic 'string'
     */
    val_set_name(val, name, xml_strlen(name));

    /* save the variable */
    res = set_str(NULL, name, xml_strlen(name), val, vartype);
    if (res != NO_ERR) {
	val_free_value(val);
    }

    return res;

}  /* var_set_from_string */


/********************************************************************
* FUNCTION var_unset
* 
* Find and remove a local or global user variable
*
* !!! This function does not try global if local fails !!!
* 
* INPUTS:
*   name == var name to unset
*   namelen == length of name string
*   vartype == variable type
*
* RETURNS:
*   status
*********************************************************************/
status_t
    var_unset (const xmlChar *name,
	       uint32 namelen,
	       var_type_t vartype)
{

    ncx_var_t *var;

#ifdef DEBUG
    if (!name) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
    if (!namelen) {
	return SET_ERROR(ERR_INTERNAL_VAL);
    }
#endif

    if (vartype == VAR_TYP_NONE || vartype > VAR_TYP_SYSTEM) {
	log_error("\nError: invalid variable type");
	return ERR_NCX_WRONG_TYPE;
    }

    var = find_var(NULL, name, namelen, 0, vartype);
    if (var && (var->vartype == VAR_TYP_SYSTEM ||
		var->vartype == VAR_TYP_CONFIG)) {
	log_error("\nError: variable cannot be removed");
	return ERR_NCX_OPERATION_FAILED;
    }

    if (var) {
	dlq_remove(var);
	free_var(var);
	return NO_ERR;
    } else {
	log_error("\nunset: Variable %s not found", name);
	return ERR_NCX_VAR_NOT_FOUND;
    }

}  /* var_unset */


/********************************************************************
* FUNCTION var_unset_que
* 
* Find and remove a Q-based user variable
* 
* INPUTS:
*   varQ == Q of ncx_var_t to use
*   name == var name to unset
*   namelen == length of name string
*   nsid == namespace ID to check if non-zero
* 
*********************************************************************/
status_t
    var_unset_que (dlq_hdr_t *varQ,
		   const xmlChar *name,
		   uint32 namelen,
		   xmlns_id_t  nsid)
{
    ncx_var_t *var;

#ifdef DEBUG
    if (!varQ || !name) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
    if (!namelen) {
	return SET_ERROR(ERR_INTERNAL_VAL);
    }
#endif

    var = remove_var(varQ, name, namelen, nsid, VAR_TYP_QUEUE);
    if (var) {
	free_var(var);
	return NO_ERR;
    } else {
	log_error("\nunset: Variable %s not found", name);
	return ERR_NCX_VAR_NOT_FOUND;
    }

}  /* var_unset_que */


/********************************************************************
* FUNCTION var_get_str
* 
* Find a global user variable
* 
* INPUTS:
*   name == var name to get
*   namelen == length of name
*   vartype == variable type
*
* RETURNS:
*   pointer to value, or NULL if not found
*********************************************************************/
val_value_t *
    var_get_str (const xmlChar *name,
		 uint32 namelen,
		 var_type_t vartype)
{
    ncx_var_t    *var;

#ifdef DEBUG
    if (!name) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }
    if (!namelen) {
	SET_ERROR(ERR_INTERNAL_VAL);
	return NULL;
    }
#endif

    if (vartype == VAR_TYP_NONE || vartype > VAR_TYP_SYSTEM) {
	return NULL;
    }

    var = find_var(NULL, name, namelen, 0, vartype);
    if (var) {
	return var->val;
    } else if (vartype == VAR_TYP_LOCAL) {
	var = find_var(NULL, name, namelen, 0, VAR_TYP_GLOBAL);
	if (var) {
	    return var->val;
	}
    }
    return NULL;

}  /* var_get_str */


/********************************************************************
* FUNCTION var_get
* 
* Find a local or global user variable
* 
* INPUTS:
*   name == var name to get
*   vartype == variable type
* 
* RETURNS:
*   pointer to value, or NULL if not found
*********************************************************************/
val_value_t *
    var_get (const xmlChar *name,
	     var_type_t vartype)
{
#ifdef DEBUG
    if (!name) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }
#endif

    return var_get_str(name, xml_strlen(name), vartype);

}  /* var_get */


/********************************************************************
* FUNCTION var_get_type_str
* 
* Find a user variable; get its var type
* 
* INPUTS:
*   name == var name to get
*   namelen == length of name
*   globalonly == TRUE to check only the global Q
*                 FALSE to check local, then global Q
*
* RETURNS:
*   var type if found, or VAR_TYP_NONE
*********************************************************************/
var_type_t
    var_get_type_str (const xmlChar *name,
		      uint32 namelen,
		      boolean globalonly)
{
    ncx_var_t    *var;

#ifdef DEBUG
    if (!name) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return VAR_TYP_NONE;
    }
    if (!namelen) {
	SET_ERROR(ERR_INTERNAL_VAL);
	return VAR_TYP_NONE;
    }
#endif

    if (!globalonly) {
	var = find_var(NULL, name, namelen, 0, VAR_TYP_LOCAL);
	if (var) {
	    return var->vartype;
	}
    } 

    var = find_var(NULL, name, namelen, 0, VAR_TYP_GLOBAL);
    if (var) {
	return var->vartype;
    }

    return VAR_TYP_NONE;

}  /* var_get_type_str */


/********************************************************************
* FUNCTION var_get_type
* 
* Get the var type of a specified var name
* 
* INPUTS:
*   name == var name to get
*   globalonly == TRUE to check only the global Q
*                 FALSE to check local, then global Q
*
* RETURNS:
*   var type or VAR_TYP_NONE if not found
*********************************************************************/
var_type_t
    var_get_type (const xmlChar *name,
		  boolean globalonly)
{
#ifdef DEBUG
    if (!name) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return VAR_TYP_NONE;
    }
#endif

    return var_get_type_str(name, 
			    xml_strlen(name),
			    globalonly);

}  /* var_get_type */


/********************************************************************
* FUNCTION var_get_str_que
* 
* Find a global user variable
* 
* INPUTS:
*   varQ == queue of ncx_var_t to use
*   name == var name to get
*   namelen == length of name
*   nsid == namespace ID  for name (0 if not used)
*
* RETURNS:
*   pointer to value, or NULL if not found
*********************************************************************/
val_value_t *
    var_get_str_que (dlq_hdr_t *varQ,
		     const xmlChar *name,
		     uint32 namelen,
		     xmlns_id_t  nsid)
{
    ncx_var_t    *var;

#ifdef DEBUG
    if (!varQ || !name) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }
    if (!namelen) {
	SET_ERROR(ERR_INTERNAL_VAL);
	return NULL;
    }
#endif

    var = find_var(varQ, name, namelen, nsid, VAR_TYP_QUEUE);
    if (var) {
	return var->val;
    } else {
	return NULL;
    }

}  /* var_get_str_que */


/********************************************************************
* FUNCTION var_get_que
* 
* Find a Q-based user variable
* 
* INPUTS:
*   varQ == Q of ncx_var_t to use
*   name == var name to get
*   nsid == namespace ID  for name (0 if not used)
* 
* RETURNS:
*   pointer to value, or NULL if not found
*********************************************************************/
val_value_t *
    var_get_que (dlq_hdr_t *varQ,
		 const xmlChar *name,
		 xmlns_id_t nsid)
{
    ncx_var_t  *var;

#ifdef DEBUG
    if (!name) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }
#endif

    var = find_var(varQ, name, xml_strlen(name), 
		   nsid, VAR_TYP_QUEUE);
    if (var) {
	return var->val;
    } else {
	return NULL;
    }

}  /* var_get_que */


/********************************************************************
* FUNCTION var_get_que_raw
* 
* Find a Q-based user variable; return the var struct instead
* of just the value
* 
* INPUTS:
*   varQ == Q of ncx_var_t to use
*   nsid == namespace ID to match (0 if not used)
*   name == var name to get
* 
* RETURNS:
*   pointer to value, or NULL if not found
*********************************************************************/
ncx_var_t *
    var_get_que_raw (dlq_hdr_t *varQ,
		     xmlns_id_t  nsid,
		     const xmlChar *name)
{
#ifdef DEBUG
    if (!name) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }
#endif

    return find_var(varQ, name, xml_strlen(name), 
		    nsid, VAR_TYP_QUEUE);

}  /* var_get_que_raw */


/********************************************************************
* FUNCTION var_get_local
* 
* Find a local user variable
* 
* INPUTS:
*   name == var name to get
* 
* RETURNS:
*   pointer to value, or NULL if not found
*********************************************************************/
val_value_t *
    var_get_local (const xmlChar *name)
{
    ncx_var_t  *var;

#ifdef DEBUG
    if (!name) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }
#endif

    var = find_var(NULL, name, xml_strlen(name), 
		   0, VAR_TYP_LOCAL);
    if (var) {
	return var->val;
    }
    return NULL;

}  /* var_get_local */


/********************************************************************
* FUNCTION var_get_local_str
* 
* Find a local user variable, count-based name string
* 
* INPUTS:
*   name == var name to get
* 
* RETURNS:
*   pointer to value, or NULL if not found
*********************************************************************/
val_value_t *
    var_get_local_str (const xmlChar *name,
		       uint32 namelen)
{
    ncx_var_t  *var;

#ifdef DEBUG
    if (!name) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }
#endif

    var = find_var(NULL, name, namelen, 0, VAR_TYP_LOCAL);
    if (var) {
	return var->val;
    }
    return NULL;

}  /* var_get_local_str */


/********************************************************************
* FUNCTION var_check_ref
* 
* Check if the immediate command sub-string is a variable
* reference.  If so, return the (vartype, name, namelen)
* tuple that identifies the reference.  Also return
* the total number of chars consumed from the input line.
* 
* E.g.,
*
*   $foo = get-config filter=@filter.xml
*
* INPUTS:
*   line == command line string to expand
*   isleft == TRUE if left hand side of an expression
*          == FALSE if right hand side ($1 type vars allowed)
*   len  == address of number chars parsed so far in line
*   vartype == address of return variable Q type
*   name == address of string start return val
*   namelen == address of name length return val
*
* OUTPUTS:
*   *len == number chars consumed by this function
*   *vartype == variable type enum
*   *name == start of name string
*   *namelen == length of *name string
*
* RETURNS:
*    status   
*********************************************************************/
status_t
    var_check_ref (const xmlChar *line,
		   var_side_t side,
		   uint32   *len,
		   var_type_t *vartype,
		   const xmlChar **name,
		   uint32 *namelen)
{
    const xmlChar  *str;
    ncx_var_t      *testvar;
    int             num;
    status_t        res;

#ifdef DEBUG
    if (!line || !len || !vartype || !name || !namelen) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    /* save start point in line */
    str = line;

    /* skip leading whitespace */
    while (*str && isspace(*str)) {
	str++;
    }

    /* check if this is not the start var char */
    if (*str != NCX_VAR_CH) {
	*len = 0;
	return NO_ERR;
    }

    /* is a var, check $$global or $local variable */
    if (str[1] == NCX_VAR_CH) {
	*vartype = VAR_TYP_GLOBAL;
	str += 2;
    } else {
	*vartype = VAR_TYP_LOCAL;
	str++;
    }

    /* check if this is a number variable reference */
    if (isdigit((int)*str)) {
	if (side==ISLEFT || *vartype == VAR_TYP_GLOBAL) {
	    *len = 0;
	    return ERR_NCX_INVALID_VALUE;
	}
	num = atoi((const char *)str);
	if (num < 0 || num > RUNSTACK_MAX_PARMS) {
	    *len = 0;
	    return ERR_NCX_INVALID_VALUE;
	}
	*namelen = 1;
    } else {
	/* parse the variable name */
	res = ncx_parse_name(str, namelen);
	if (res != NO_ERR) {
	    *len = 0;
	    return res;
	}
    }

    /* return the name string */
    *name = str;
    str += *namelen;
    *len = (uint32)(str - line);

    /* check the global var further */
    if (*vartype == VAR_TYP_GLOBAL) {
	/* VAR_TYP_GLOBAL selects anything in the globalQ */
	testvar = find_var(NULL, *name, *namelen, 
			   0, VAR_TYP_GLOBAL);
	if (testvar) {
	    /* could be VAR_TYP_SYSTEM, VAR_TYP_CONFIG,
	     * or VAR_TYP_GLOBAL
	     */
	    *vartype = testvar->vartype;
	}
    }

    return NO_ERR;

}  /* var_check_ref */


/********************************************************************
* FUNCTION var_get_script_val
* 
* Create or fill in a val_value_t struct for a parameter assignment
* within the script processing mode
*
* See ncxcli.c for details on the script syntax
*
* INPUTS:
*   obj == expected type template 
*          == NULL and will be set to NCX_BT_STRING for
*             simple types
*   val == value to fill in :: val->obj MUST be set
*          == NULL to create a new one
*   strval == string value to check
*   istop == TRUE (ISTOP) if calling from top level assignment
*            An unquoted string is the start of a command
*         == FALSE (ISPARM) if calling from a parameter parse
*            An unquoted string is just a string
*   res == address of status result
*
* OUTPUTS:
*   *res == status
*
* RETURNS:
*   If error, then returns NULL;
*   If no error, then returns pointer to new val or filled in 'val'
*********************************************************************/
val_value_t *
    var_get_script_val (const obj_template_t *obj,
			val_value_t *val,
			const xmlChar *strval,
			boolean istop,
			status_t *res)
{
    val_value_t           *varval;
    const xmlChar         *str, *name;
    val_value_t           *newval, *useval;

    xmlChar               *fname, *intbuff;
    uint32                 namelen, len;
    boolean                isvarref;
    var_type_t             vartype;

#ifdef DEBUG
    if (!obj || !res) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }
#endif

    newval = NULL;
    isvarref = FALSE;

    *res = NO_ERR;

    /* get a new value struct if one is not provided */
    if (strval && *strval == NCX_VAR_CH) {
	isvarref = TRUE;
    }

    if (val) {
	useval = val;
    } else {
	newval = val_new_value();
	if (!newval) {
	    *res = ERR_INTERNAL_MEM;
	    return NULL;
	} else {
	    useval = newval;
	}
    }

    /* check if strval is NULL */
    if (!strval) {
	*res = val_set_simval(useval,
			      obj_get_ctypdef(obj),
			      obj_get_nsid(obj),
			      obj_get_name(obj),
			      NULL);
    } else if (*strval==NCX_AT_CH) {
	/* this is a NCX_BT_EXTERNAL value
	 * find the file with the raw XML data
	 */
	fname = ncxmod_find_data_file(&strval[1], TRUE, res);
	if (fname) {
	    /* hand off the malloced 'fname' to be freed later */
	    val_set_extern(useval, fname);
	    
	} /* else res already set */
    } else if (*strval == NCX_VAR_CH) {
	/* this is a variable reference
	 * get the value and clone it for the new value
	 * flag an error if variable not found
	 */
	*res = var_check_ref(strval, ISRIGHT, &len, &vartype, 
			     &name, &namelen);
	if (*res == NO_ERR) {
	    varval = var_get_str(name, namelen, vartype);
	    if (!varval) {
		*res = ERR_NCX_VAR_NOT_FOUND;
	    } else if (typ_is_simple(varval->btyp)) {
		*res = val_replace(varval, useval);
	    } else {
		*res = ERR_NCX_WRONG_DATATYP;
	    }
	}
    } else if (*strval == NCX_QUOTE_CH) {
	/* this is a quoted string literal */
	/* set the start after quote */
	str = ++strval;

	/* find the end of the quoted string */
	while (*str && *str != NCX_QUOTE_CH) {
	    str++;
	}
	*res = val_set_string2(useval, 
			       obj_get_name(obj), 
			       obj_get_ctypdef(obj), 
			       strval, (uint32)(str-strval)); 
    } else if ((*strval == NCX_XML1a_CH) &&
	       (strval[1] == NCX_XML1b_CH)) {

	/* this is a bracketed inline XML sequence */
	str = strval+2;
			    
	/* find the end of the inline XML */
	while (*str && 
	       !((*str==NCX_XML2a_CH) && (str[1]==NCX_XML2b_CH))) {
	    str++;
	}
	intbuff = xml_strndup(strval+1, (uint32)(str-strval));
	if (!intbuff) {
	    *res = ERR_INTERNAL_MEM;
	} else {
	    val_set_intern(useval, intbuff);
	}
    } else if (istop && ncx_valid_fname_ch(*strval)) {
	/* this is a regular string, treated as a function
	 * call at the top level.  If no RPC method is found,
	 * then it will be treated as a string
	 */
	*res = NO_ERR;
	if (newval) {
	    val_free_value(newval);
	}
	return NULL;
    } else {
	/* this is a regular string, but not a valid NcxName,
	 * so just treat as a string instead of potential RPC method
	 */
	*res = val_set_simval(useval, 
			      obj_get_ctypdef(obj), 
			      val_get_nsid(useval), 
			      useval->name, 
			      strval);
    }

    /* clean up and exit */
    if (*res != NO_ERR) {
	if (newval) {
	    val_free_value(newval);
	}
	useval = NULL;
    }
    return useval;

}  /* var_get_script_val */


/********************************************************************
* FUNCTION var_check_script_val
* 
* Create a val_value_t struct for a parameter assignment
* within the script processing mode, if a var ref is found
*
* See yangcli documentation for details on the script syntax
*
* INPUTS:
*   obj == expected object template 
*          == NULL and will be set to NCX_BT_STRING for
*             simple types
*   strval == string value to check
*   istop == TRUE if calling from top level assignment
*            An unquoted string is the start of a command
*         == FALSE if calling from a parameter parse
*            An unquoted string is just a string
*   res == address of status result
*
* OUTPUTS:
*   *res == status
*
* RETURNS:
*   If no error, then returns pointer to new malloced val 
*   If error, then returns NULL
*********************************************************************/
val_value_t *
    var_check_script_val (const obj_template_t *obj,
			  const xmlChar *strval,
			  boolean istop,
			  status_t *res)
{
    const obj_template_t  *useobj;
    const val_value_t     *varval;
    const xmlChar         *str, *name;
    val_value_t           *newval;
    xmlChar               *fname, *intbuff;
    uint32                 namelen, len;
    var_type_t             vartype;

#ifdef DEBUG
    if (!res || !strval) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }
#endif

    useobj = NULL;
    newval = NULL;

    *res = NO_ERR;

    /* get a new value struct if one is not provided */
    if (*strval == NCX_VAR_CH) {
	/* this is a variable reference
	 * get the value and clone it for the new value
	 * flag an error if variable not found
	 */
	*res = var_check_ref(strval, ISRIGHT, &len, &vartype, 
			     &name, &namelen);
	if (*res == NO_ERR) {
	    varval = var_get_str(name, namelen, vartype);
	    if (!varval) {
		*res = ERR_NCX_DEF_NOT_FOUND;
	    } else {
		newval = val_clone(varval);
		if (!newval) {
		    *res = ERR_INTERNAL_MEM;
		}
	    }
	}
	return newval;
    }

    /* not a variable reference so check further */
    if (obj) {
	useobj = obj;
    } else {
	useobj = ncx_get_gen_string();
	if (!useobj) {
	    *res = SET_ERROR(ERR_INTERNAL_VAL);
	    return NULL;
	}
    }

    /* malloc and init a new value struct for the result */
    newval = val_new_value();
    if (!newval) {
	*res = ERR_INTERNAL_MEM;
	return NULL;
    }
    val_init_from_template(newval, useobj);

    /* check the string for the appopriate value assignment */
    if (*strval==NCX_AT_CH) {
	/* this is a NCX_BT_EXTERNAL value
	 * find the file with the raw XML data
	 */
	fname = ncxmod_find_data_file(&strval[1], TRUE, res);
	if (fname) {
	    /* hand off the malloced 'fname' to be freed later */
	    val_set_extern(newval, fname);
	    
	} /* else res already set */
    } else if (strval && *strval == NCX_QUOTE_CH) {
	/* this is a quoted string literal */
	/* set the start after quote */
	str = ++strval;

	/* find the end of the quoted string */
	while (*str && *str != NCX_QUOTE_CH) {
	    str++;
	}
	*res = val_set_string2(newval, NULL, 
			       obj_get_ctypdef(useobj), 
			       strval, (uint32)(str-strval)); 
    } else if (strval && (*strval == NCX_XML1a_CH) &&
	       (strval[1] == NCX_XML1b_CH)) {

	/* this is a bracketed inline XML sequence */
	str = strval+2;
			    
	/* find the end of the inline XML */
	while (*str && 
	       !((*str==NCX_XML2a_CH) && (str[1]==NCX_XML2b_CH))) {
	    str++;
	}
	intbuff = xml_strndup(strval+1, (uint32)(str-strval));
	if (!intbuff) {
	    *res = ERR_INTERNAL_MEM;
	} else {
	    val_set_intern(newval, intbuff);
	}
    } else if (strval && istop && ncx_valid_fname_ch(*strval)) {
	/* this is a regular string, treated as a function
	 * call at the top level; return NULL but with
	 * the res status set to NO_ERR to signal that
	 * an RPC method needs to be checked
	 */
	*res = NO_ERR;
	val_free_value(newval);
	return NULL;
    } else {
	/* this is a regular string, treated as a string
	 * when used within an RPC function  parameter
	 */
	*res = val_set_simval(newval,
			      obj_get_ctypdef(useobj), 
			      obj_get_nsid(useobj),
			      obj_get_name(useobj), 
			      strval);
    }

    /* tried to get some value set, only return value if NO_ERR 
     *  TBD: extended error reporting by returning the value anyways
     */
    if (*res != NO_ERR) {
	val_free_value(newval);
	newval = NULL;
    }
    return newval;

}  /* var_check_script_val */




/* END var.c */
