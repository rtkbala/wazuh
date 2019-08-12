/* Copyright (C) 2015-2019, Wazuh Inc.
 * Copyright (C) 2009 Trend Micro Inc.
 * All right reserved.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation
 */

#include "shared.h"
#include "rules.h"
#include "eventinfo.h"

/* Rulenode local  */
RuleNode *rulenode;

/* _OS_Addrule: Internal AddRule */
static RuleNode *_OS_AddRule(RuleNode *_rulenode, RuleInfo *read_rule);
static int _AddtoRule(int sid, int level, int none, const char *group,
               RuleNode *r_node, RuleInfo *read_rule);


/* Create the RuleList */
void OS_CreateRuleList()
{
    rulenode = NULL;
    return;
}

/* Get first node from rule */
RuleNode *OS_GetFirstRule()
{
    RuleNode *rulenode_pt = rulenode;
    return (rulenode_pt);
}

/* Search all rules, including children */
static int _AddtoRule(int sid, int level, int none, const char *group,
               RuleNode *r_node, RuleInfo *read_rule)
{
    int r_code = 0;

    /* If we don't have the first node, start from
     * the beginning of the list
     */
    if (!r_node) {
        r_node = OS_GetFirstRule();
    }

    while (r_node) {
        /* Check if the sigid matches */
        if (sid) {
            if (r_node->ruleinfo->sigid == sid) {
                /* Assign the category of this rule to the child
                 * as they must match
                 */
                read_rule->category = r_node->ruleinfo->category;

                r_node->child =
                    _OS_AddRule(r_node->child, read_rule);
                return (1);
            }
        }

        /* Check if the group matches */
        else if (group) {
            if (OS_WordMatch(group, r_node->ruleinfo->group) &&
                    (r_node->ruleinfo->sigid != read_rule->sigid)) {
                /* Loop over all rules until we find it */
                r_node->child =
                    _OS_AddRule(r_node->child, read_rule);
                r_code = 1;
            }
        }

        /* Check if the level matches */
        else if (level) {
            if ((r_node->ruleinfo->level >= level) &&
                    (r_node->ruleinfo->sigid != read_rule->sigid)) {
                r_node->child =
                    _OS_AddRule(r_node->child, read_rule);
                r_code = 1;
            }
        }

        /* If we are not searching for the sid/group, the category must
         * be the same
         */
        else if (read_rule->category != r_node->ruleinfo->category) {
            r_node = r_node->next;
            continue;
        }

        /* If none of them are set, add for the category */
        else {
            /* Set the parent category to it */
            read_rule->category = r_node->ruleinfo->category;
            r_node->child =
                _OS_AddRule(r_node->child, read_rule);
            return (1);
        }

        /* Check if the child has a rule */
        if (r_node->child) {
            if (_AddtoRule(sid, level, none, group, r_node->child, read_rule)) {
                r_code = 1;
            }
        }

        r_node = r_node->next;
    }

    return (r_code);
}

/* Add a child */
int OS_AddChild(RuleInfo *read_rule)
{
    if (!read_rule) {
        merror("rules_list: Passing a NULL rule. Inconsistent state");
        return (1);
    }

    /* Adding for if_sid */
    if (read_rule->if_sid) {
        int val = 0;
        const char *sid;

        sid  = read_rule->if_sid;

        /* Loop to read all the rules (comma or space separated) */
        do {
            int rule_id = 0;
            if ((*sid == ',') || (*sid == ' ')) {
                val = 0;
                continue;
            } else if ((isdigit((int)*sid)) || (*sid == '\0')) {
                if (val == 0) {
                    rule_id = atoi(sid);
                    if (!_AddtoRule(rule_id, 0, 0, NULL, NULL, read_rule)) {
                        merror_exit("rules_list: Signature ID '%d' not "
                                  "found. Invalid 'if_sid'.", rule_id);
                    }
                    val = 1;
                }
            } else {
                merror_exit("rules_list: Signature ID must be an integer. "
                          "Exiting...");
            }
        } while (*sid++ != '\0');
    }

    /* Adding for if_level */
    else if (read_rule->if_level) {
        int  ilevel = 0;

        ilevel = atoi(read_rule->if_level);
        if (ilevel == 0) {
            merror("Invalid level (atoi)");
            return (1);
        }

        ilevel *= 100;

        if (!_AddtoRule(0, ilevel, 0, NULL, NULL, read_rule)) {
            merror_exit("rules_list: Level ID '%d' not "
                      "found. Invalid 'if_level'.", ilevel);
        }
    }

    /* Adding for if_group */
    else if (read_rule->if_group) {
        if (!_AddtoRule(0, 0, 0, read_rule->if_group, NULL, read_rule)) {
            merror_exit("rules_list: Group '%s' not "
                      "found. Invalid 'if_group'.", read_rule->if_group);
        }
    }

    /* Just add based on the category */
    else {
        if (!_AddtoRule(0, 0, 0, NULL, NULL, read_rule)) {
            merror_exit("rules_list: Category '%d' not "
                      "found. Invalid 'category'.", read_rule->category);
        }
    }

    /* done over here */
    return (0);
}

/* Add a rule in the chain */
static RuleNode *_OS_AddRule(RuleNode *_rulenode, RuleInfo *read_rule)
{
    RuleNode *tmp_rulenode = _rulenode;

    if (tmp_rulenode != NULL) {
        int middle_insertion = 0;
        RuleNode *prev_rulenode = NULL;
        RuleNode *new_rulenode = NULL;

        while (tmp_rulenode != NULL) {
            if (read_rule->level > tmp_rulenode->ruleinfo->level) {
                middle_insertion = 1;
                break;
            }
            prev_rulenode = tmp_rulenode;
            tmp_rulenode = tmp_rulenode->next;
        }

        new_rulenode = (RuleNode *)calloc(1, sizeof(RuleNode));

        if (!new_rulenode) {
            merror_exit(MEM_ERROR, errno, strerror(errno));
        }

        if (middle_insertion == 1) {
            if (prev_rulenode == NULL) {
                _rulenode = new_rulenode;
            } else {
                prev_rulenode->next = new_rulenode;
            }

            new_rulenode->next = tmp_rulenode;
            new_rulenode->ruleinfo = read_rule;
            new_rulenode->child = NULL;
        } else {
            prev_rulenode->next = new_rulenode;
            prev_rulenode->next->ruleinfo = read_rule;
            prev_rulenode->next->next = NULL;
            prev_rulenode->next->child = NULL;
        }
    } else {
        _rulenode = (RuleNode *)calloc(1, sizeof(RuleNode));
        if (_rulenode == NULL) {
            merror_exit(MEM_ERROR, errno, strerror(errno));
        }

        _rulenode->ruleinfo = read_rule;
        _rulenode->next = NULL;
        _rulenode->child = NULL;
    }

    return (_rulenode);
}

/* External AddRule */
int OS_AddRule(RuleInfo *read_rule)
{
    rulenode = _OS_AddRule(rulenode, read_rule);

    return (0);
}

/**
 * @brief Add the Rulenode c as child of new_p
 * @param c Child rule
 * @param old_p Old parent rule
 * @param new_p New parent rule
 */
void modifyParent (RuleNode *c, RuleNode *old_p, RuleNode *new_p)
{
    RuleNode *tmp = NULL, *prev = NULL;

    if(old_p->child->ruleinfo->sigid == c->ruleinfo->sigid) {
        old_p->child = old_p->child->next;
    }
    else{
        tmp = old_p->child;
        while (tmp != NULL && tmp->ruleinfo->sigid != c->ruleinfo->sigid) {
            prev = tmp;
            tmp = tmp->next;
        }
        prev->next = tmp->next;
    }

    if (new_p->child != NULL) {
        tmp = new_p->child;
        prev = NULL;

        while (tmp != NULL) {
            prev = tmp;
            tmp = tmp->next;
        }
        prev->next = c;
    }
    else {
        new_p->child = c;
    }

    c->next = NULL;

}

/**
 * @brief Get rule node
 * @param sigid rule id
 * @param array array of rules
 * @return node pointer to rule node
 */
RuleNode *getRule (int sigid, RuleNode *array){
    RuleNode *tmp = array, *node = NULL;

    while(tmp != NULL){
        if (tmp->ruleinfo->sigid == sigid) {
            return tmp;
        }
        else if (tmp->child){
            node = getRule(sigid, tmp->child);
            if(node != NULL){
                return node;
            }
        }

        tmp = tmp->next;
    }

    return node;
}

/**
 * @brief Get root node
 * @param category Rule category
 * @return node pointer to rule node
 */
RuleNode *getInitialNode(u_int8_t category){
    RuleNode *node = rulenode;

    while(node != NULL && node->ruleinfo->category != category)
        node = node->next;

    return node;
}

/**
 * @brief Update rule info for overwritten ones
 * @param r_node rule to modify
 * @param newrule the new values of the rule
 * @param sid rule id
 * @return Success 1 if not success 0
 */
int OS_AddRuleInfo(RuleNode *r_node, RuleInfo *newrule, int sid)
{
    /* If no r_node is given, get first node */
    if (r_node == NULL) {
        r_node = OS_GetFirstRule();
    }

    if (sid == 0) {
        return (0);
    }

    while (r_node) {
        /* Check if the sigid matches */
        if (r_node->ruleinfo->sigid == sid) {

            /* Parent will be modified if category or if_sid are diferents */
            RuleNode *new_f = NULL, *old_f = NULL;

            if (r_node->ruleinfo->if_sid != NULL && newrule->if_sid != NULL
                && strcmp(r_node->ruleinfo->if_sid, newrule->if_sid) != 0) {
                old_f = getRule(atoi(r_node->ruleinfo->if_sid), rulenode);
                new_f = getRule(atoi(newrule->if_sid), rulenode);
            }
            else if (r_node->ruleinfo->if_sid != NULL && newrule->if_sid == NULL) {
                old_f = getRule(atoi(r_node->ruleinfo->if_sid), rulenode);
                new_f = getInitialNode(newrule->category);
            }
            else if (r_node->ruleinfo->if_sid == NULL && newrule->if_sid != NULL){
                old_f = getInitialNode(r_node->ruleinfo->category);
                new_f = getRule(atoi(newrule->if_sid), rulenode);
            }
            else if(r_node->ruleinfo->category != newrule->category) {
                old_f = getInitialNode(r_node->ruleinfo->category);
                new_f = getInitialNode(newrule->category);
            }

            if(new_f != NULL && old_f != NULL) {
                modifyParent(r_node, old_f, new_f);
            }

            /* Assign new values */
            r_node->ruleinfo->category = newrule->category;
            r_node->ruleinfo->if_sid = newrule->if_sid;
            r_node->ruleinfo->if_level = newrule->if_level;
            r_node->ruleinfo->if_group = newrule->if_group;

            r_node->ruleinfo->level = newrule->level;
            r_node->ruleinfo->maxsize = newrule->maxsize;
            r_node->ruleinfo->frequency = newrule->frequency;
            r_node->ruleinfo->timeframe = newrule->timeframe;

            r_node->ruleinfo->firedtimes = newrule->firedtimes;
            r_node->ruleinfo->time_ignored = newrule->time_ignored;
            r_node->ruleinfo->ignore_time = newrule->ignore_time;
            r_node->ruleinfo->ignore = newrule->ignore;
            r_node->ruleinfo->ckignore = newrule->ckignore;
            r_node->ruleinfo->ignore_fields = newrule->ignore_fields;
            r_node->ruleinfo->ckignore_fields = newrule->ckignore_fields;
            r_node->ruleinfo->group_prev_matched_sz = newrule->group_prev_matched_sz;

            r_node->ruleinfo->sid_prev_matched = newrule->sid_prev_matched;
            r_node->ruleinfo->sid_search = newrule->sid_search;
            r_node->ruleinfo->group_prev_matched = newrule->group_prev_matched;
            r_node->ruleinfo->group_search = newrule->group_search;
            r_node->ruleinfo->event_search = newrule->event_search;

            r_node->ruleinfo->group = newrule->group;
            r_node->ruleinfo->match = newrule->match;
            r_node->ruleinfo->regex = newrule->regex;
            r_node->ruleinfo->day_time = newrule->day_time;
            r_node->ruleinfo->week_day = newrule->week_day;
            r_node->ruleinfo->srcip = newrule->srcip;
            r_node->ruleinfo->dstip = newrule->dstip;
            r_node->ruleinfo->srcport = newrule->srcport;
            r_node->ruleinfo->dstport = newrule->dstport;
            r_node->ruleinfo->user = newrule->user;
            r_node->ruleinfo->url = newrule->url;
            r_node->ruleinfo->id = newrule->id;
            r_node->ruleinfo->status = newrule->status;
            r_node->ruleinfo->hostname = newrule->hostname;
            r_node->ruleinfo->program_name = newrule->program_name;
            r_node->ruleinfo->extra_data = newrule->extra_data;
            r_node->ruleinfo->fields = newrule->fields;
            r_node->ruleinfo->action = newrule->action;
            r_node->ruleinfo->comment = newrule->comment;
            r_node->ruleinfo->info = newrule->info;
            r_node->ruleinfo->cve = newrule->cve;
            r_node->ruleinfo->info_details = newrule->info_details;
            r_node->ruleinfo->lists = newrule->lists;
            r_node->ruleinfo->if_matched_regex = newrule->if_matched_regex;
            r_node->ruleinfo->if_matched_group = newrule->if_matched_group;
            r_node->ruleinfo->if_matched_sid = newrule->if_matched_sid;
            r_node->ruleinfo->file = newrule->file;
            r_node->ruleinfo->alert_opts = newrule->alert_opts;
            r_node->ruleinfo->context_opts = newrule->context_opts;
            r_node->ruleinfo->context = newrule->context;
            r_node->ruleinfo->decoded_as = newrule->decoded_as;
            r_node->ruleinfo->ar = newrule->ar;
            r_node->ruleinfo->compiled_rule = newrule->compiled_rule;

            r_node->ruleinfo->mutex = newrule->mutex;
            r_node->ruleinfo->location = newrule->location;
            r_node->ruleinfo->lists = newrule->lists;
            r_node->ruleinfo->prev_rule = newrule->prev_rule;
            r_node->ruleinfo->same_fields = newrule->same_fields;
            r_node->ruleinfo->not_same_fields = newrule->not_same_fields;

#ifdef LIBGEOIP_ENABLED
            r_node->ruleinfo->srcgeoip = newrule->srcgeoip;
            r_node->ruleinfo->dstgeoip = newrule->dstgeoip;
#endif

            return (1);
        }


        /* Check if the child has a rule */
        if (r_node->child) {
            if (OS_AddRuleInfo(r_node->child, newrule, sid)) {
                return (1);
            }
        }

        r_node = r_node->next;
    }

    return (0);
}

/**
 * @brief Mark rules that match specific id (for if_matched_sid)
 * @param r_node rule to mark
 * @param orig_rule orig_rule rule that match if specific rule do
 * @return 0
 */
int OS_MarkID(RuleNode *r_node, RuleInfo *orig_rule)
{
    /* If no r_node is given, get first node */
    if (r_node == NULL) {
        r_node = OS_GetFirstRule();
    }

    while (r_node) {
        if (r_node->ruleinfo->sigid == orig_rule->if_matched_sid) {
            /* If child does not have a list, create one */
            if (!r_node->ruleinfo->sid_prev_matched) {
                r_node->ruleinfo->sid_prev_matched = OSList_Create();
                if (!r_node->ruleinfo->sid_prev_matched) {
                    merror_exit(MEM_ERROR, errno, strerror(errno));
                }
                //OSList_SetFreeDataPointer(r_node->ruleinfo->sid_prev_matched, (void (*)(void *)) Free_Eventinfo);
            }

            /* Assign the parent pointer to it */
            orig_rule->sid_search = r_node->ruleinfo->sid_prev_matched;
        }

        /* Check if the child has a rule */
        if (r_node->child) {
            OS_MarkID(r_node->child, orig_rule);
        }

        r_node = r_node->next;
    }

    return (0);
}

/**
 * @brief Mark rules that match specific group (for if_matched_group)
 * @param r_node rule to mark
 * @param orig_rule rule that match if specific group do
 * @return 0
 */
int OS_MarkGroup(RuleNode *r_node, RuleInfo *orig_rule)
{
    /* If no r_node is given, get first node */
    if (r_node == NULL) {
        r_node = OS_GetFirstRule();
    }

    while (r_node) {
        if (OSMatch_Execute(r_node->ruleinfo->group,
                            strlen(r_node->ruleinfo->group),
                            orig_rule->if_matched_group)) {
            unsigned int rule_g = 0;
            if (r_node->ruleinfo->group_prev_matched) {
                while (r_node->ruleinfo->group_prev_matched[rule_g]) {
                    rule_g++;
                }
            }

            os_realloc(r_node->ruleinfo->group_prev_matched,
                       (rule_g + 2)*sizeof(OSList *),
                       r_node->ruleinfo->group_prev_matched);

            r_node->ruleinfo->group_prev_matched[rule_g] = NULL;
            r_node->ruleinfo->group_prev_matched[rule_g + 1] = NULL;

            /* Set the size */
            r_node->ruleinfo->group_prev_matched_sz = rule_g + 1;

            r_node->ruleinfo->group_prev_matched[rule_g] =
                orig_rule->group_search;
        }

        /* Check if the child has a rule */
        if (r_node->child) {
            OS_MarkGroup(r_node->child, orig_rule);
        }

        r_node = r_node->next;
    }

    return (0);
}
