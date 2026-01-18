/*
 * chan_mobile_cli.h - CLI command handlers for chan_mobile
 */

#ifndef CHAN_MOBILE_CLI_H
#define CHAN_MOBILE_CLI_H

#include "asterisk/cli.h"

/* CLI command handlers */
char *handle_cli_mobile_show_devices(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
char *handle_cli_mobile_show_device(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
char *handle_cli_mobile_show_adapters(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
char *handle_cli_mobile_show_stats(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
char *handle_cli_mobile_show_version(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
char *handle_cli_mobile_search(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
char *handle_cli_mobile_rfcomm(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
char *handle_cli_mobile_cusd(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
char *handle_cli_mobile_reset_backoff(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
char *handle_cli_mobile_reset_stats(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
char *handle_cli_mobile_check(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
char *handle_cli_mobile_disconnect(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);

/* CLI entry array - defined in chan_mobile_cli.c */
extern struct ast_cli_entry mbl_cli[];
extern int mbl_cli_count;

/* Initialize/cleanup CLI */
int mbl_cli_init(void);
void mbl_cli_cleanup(void);

#endif /* CHAN_MOBILE_CLI_H */
