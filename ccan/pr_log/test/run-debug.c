#define DEBUG 1
#include <ccan/pr_log/pr_log.h>
#include <ccan/pr_log/pr_log.c>
#include <ccan/tap/tap.h>

int main(void)
{
	plan_tests(6);
	debug = 3;
	ok1(!debug_is(4));
	debug = 1;
	ok1(debug_is(0));
	ok1(debug_is(1));
	ok1(!debug_is(2));
	ok1(!debug_is(3));

	pr_emerg("Emerg\n");
	pr_alert("Alert\n");
	pr_crit("Crit\n");
	pr_error("Error\n");
	pr_warn("Warn\n");
	pr_notice("Notice\n");
	pr_info("Info\n");
	pr_debug("Debug\n");

	pr_devel("Devel\n");

	debug = INT_MIN;
	setenv("DEBUG", "1", 1);
	ok1(debug_is(1));

	/* malformed check */
	pr_log_(":4> 1\n");
	pr_log_("<!> 2\n");
	pr_log_("<1} 3\n");

	return exit_status();
}
