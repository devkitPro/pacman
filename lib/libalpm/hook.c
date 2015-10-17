/*
 *  hook.c
 *
 *  Copyright (c) 2015 Pacman Development Team <pacman-dev@archlinux.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>

#include "handle.h"
#include "hook.h"
#include "log.h"
#include "util.h"

enum _alpm_hook_op_t {
	ALPM_HOOK_OP_INSTALL = (1 << 0),
	ALPM_HOOK_OP_UPGRADE = (1 << 1),
	ALPM_HOOK_OP_REMOVE = (1 << 2),
};

enum _alpm_trigger_type_t {
	ALPM_HOOK_TYPE_PACKAGE = 1,
	ALPM_HOOK_TYPE_FILE,
};

struct _alpm_trigger_t {
	enum _alpm_hook_op_t op;
	enum _alpm_trigger_type_t type;
	alpm_list_t *targets;
};

struct _alpm_hook_t {
	char *name;
	alpm_list_t *triggers;
	alpm_list_t *depends;
	char *cmd;
	enum _alpm_hook_when_t when;
	int abort_on_fail;
};

struct _alpm_hook_cb_ctx {
	alpm_handle_t *handle;
	struct _alpm_hook_t *hook;
};

static void _alpm_trigger_free(struct _alpm_trigger_t *trigger)
{
	if(trigger) {
		FREELIST(trigger->targets);
		free(trigger);
	}
}

static void _alpm_hook_free(struct _alpm_hook_t *hook)
{
	if(hook) {
		free(hook->name);
		free(hook->cmd);
		alpm_list_free_inner(hook->triggers, (alpm_list_fn_free) _alpm_trigger_free);
		alpm_list_free(hook->triggers);
		FREELIST(hook->depends);
		free(hook);
	}
}

static int _alpm_hook_parse_cb(const char *file, int line,
		const char *section, char *key, char *value, void *data)
{
	struct _alpm_hook_cb_ctx *ctx = data;
	alpm_handle_t *handle = ctx->handle;
	struct _alpm_hook_t *hook = ctx->hook;

#define error(...) _alpm_log(handle, ALPM_LOG_ERROR, __VA_ARGS__); return 1;

	if(!section && !key) {
		error(_("error while reading hook %s: %s\n"), file, strerror(errno));
	} else if(!section) {
		error(_("hook %s line %d: invalid option %s\n"), file, line, key);
	} else if(!key) {
		/* beginning a new section */
		if(strcmp(section, "Trigger") == 0) {
			struct _alpm_trigger_t *t;
			CALLOC(t, sizeof(struct _alpm_trigger_t), 1, return 1);
			hook->triggers = alpm_list_add(hook->triggers, t);
		} else if(strcmp(section, "Action") == 0) {
			/* no special processing required */
		} else {
			error(_("hook %s line %d: invalid section %s\n"), file, line, section);
		}
	} else if(strcmp(section, "Trigger") == 0) {
		struct _alpm_trigger_t *t = hook->triggers->prev->data;
		if(strcmp(key, "Operation") == 0) {
			if(strcmp(value, "Install") == 0) {
				t->op |= ALPM_HOOK_OP_INSTALL;
			} else if(strcmp(value, "Upgrade") == 0) {
				t->op |= ALPM_HOOK_OP_UPGRADE;
			} else if(strcmp(value, "Remove") == 0) {
				t->op |= ALPM_HOOK_OP_REMOVE;
			} else {
				error(_("hook %s line %d: invalid value %s\n"), file, line, value);
			}
		} else if(strcmp(key, "Type") == 0) {
			if(strcmp(value, "Package") == 0) {
				t->type = ALPM_HOOK_TYPE_PACKAGE;
			} else if(strcmp(value, "File") == 0) {
				t->type = ALPM_HOOK_TYPE_FILE;
			} else {
				error(_("hook %s line %d: invalid value %s\n"), file, line, value);
			}
		} else if(strcmp(key, "Target") == 0) {
			char *val;
			STRDUP(val, value, return 1);
			t->targets = alpm_list_add(t->targets, val);
		} else {
			error(_("hook %s line %d: invalid option %s\n"), file, line, key);
		}
	} else if(strcmp(section, "Action") == 0) {
		if(strcmp(key, "When") == 0) {
			if(strcmp(value, "PreTransaction") == 0) {
				hook->when = ALPM_HOOK_PRE_TRANSACTION;
			} else if(strcmp(value, "PostTransaction") == 0) {
				hook->when = ALPM_HOOK_POST_TRANSACTION;
			} else {
				error(_("hook %s line %d: invalid value %s\n"), file, line, value);
			}
		} else if(strcmp(key, "Depends") == 0) {
			char *val;
			STRDUP(val, value, return 1);
			hook->depends = alpm_list_add(hook->depends, val);
		} else if(strcmp(key, "AbortOnFail") == 0) {
			hook->abort_on_fail = 1;
		} else if(strcmp(key, "Exec") == 0) {
			STRDUP(hook->cmd, value, return 1);
		} else {
			error(_("hook %s line %d: invalid option %s\n"), file, line, value);
		}
	}

#undef error

	return 0;
}

/* vim: set noet: */
