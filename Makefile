################################################################################
# Makefile                                                                     #
#                                                                              #
# Description: This file contains the make rules for Recitation 1.             #
#                                                                              #
# Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                          #
#          Wolf Richter <wolf@cs.cmu.edu>                                      #
#                                                                              #
################################################################################

# default: echo_client lisod

# echo_client:
# 	@gcc echo_client.c -o echo_client -Wall -Werror

# liso:
# 	@gcc lisod.c log.c -o lisod -Wall -Werror -lefence

# clean:
# 	@rm echo_client lisod

CC = gcc
CFLAGS = -Wall -Werror
EXES = lisod 

all: $(EXES)

lisod:
	$(CC) $(CFLAGS) lisod.c log.c -o lisod

clean:
	@rm -rf $(EXES) lisod.log lisod.lock