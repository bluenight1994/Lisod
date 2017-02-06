################################################################################
# Makefile                                                                     #
#                                                                              #
# Description: This file contains the make rules for Recitation 1.             #
#                                                                              #
# Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                          #
#          Wolf Richter <wolf@cs.cmu.edu>                                      #
#                                                                              #
################################################################################

default: echo_server echo_client lisod

echo_server:
	@gcc echo_server.c -o echo_server -Wall -Werror
echo_client:
	@gcc echo_client.c -o echo_client -Wall -Werror

liso:
	@gcc lisod.c -o liso -Wall -Werror

clean:
	@rm echo_server echo_client lisod
