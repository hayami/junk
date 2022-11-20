#!/bin/sh
exec printf '\e[31;1mFAIL\e[0m%s\n' "$@"

# vim: et sw=4 sts=4
