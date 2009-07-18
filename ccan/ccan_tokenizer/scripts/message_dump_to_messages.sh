#!/bin/sh
sed 's/^D: /{.level=TM_DEBUG, .path="/' |
sed 's/^I: /{.level=TM_INFO, .path="/' |
sed 's/^W: /{.level=TM_WARN, .path="/' |
sed 's/^BUG: /{.level=TM_BUG, .path="/' |
sed 's/:.*/\"},/'
