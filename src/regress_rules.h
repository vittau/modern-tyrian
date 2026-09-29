#ifndef REGRESS_RULES_H
#define REGRESS_RULES_H

// Code-owned rule fixtures; inert unless --regress-rules accompanies a level run.
extern const char *regress_rule_fixture;
void regress_rules_run(void);

#endif
