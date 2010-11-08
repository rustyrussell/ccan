#include <ccan/iscsi/iscsi.h>
#include <ccan/iscsi/discovery.c>
#include <ccan/iscsi/socket.c>
#include <ccan/iscsi/init.c>
#include <ccan/iscsi/pdu.c>
#include <ccan/iscsi/scsi-lowlevel.c>
#include <ccan/iscsi/nop.c>
#include <ccan/iscsi/login.c>
#include <ccan/iscsi/scsi-command.c>
#include <ccan/tap/tap.h>

int main(void)
{
	struct iscsi_context *iscsi;

	plan_tests(2);

	iscsi = iscsi_create_context("some name");
	ok1(iscsi);
	ok1(iscsi_destroy_context(iscsi) == 0);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
