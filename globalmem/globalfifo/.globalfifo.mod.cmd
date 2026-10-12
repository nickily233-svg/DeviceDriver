savedcmd_globalfifo.mod := printf '%s\n'   globalfifo.o | awk '!x[$$0]++ { print("./"$$0) }' > globalfifo.mod
