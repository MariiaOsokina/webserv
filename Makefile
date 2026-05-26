# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: aistok <aistok@student.42london.com>       +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2026/01/14 18:42:19 by aistok            #+#    #+#              #
#    Updated: 2026/05/26 12:52:22 by aistok           ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

CC			=	c++
CFLAGS		=	-Wall -Werror -Wextra -std=c++98
#DFLAGS		=	-g3 -O0
#DFLAGS		=	-fsanitise=address
RM			=	rm -rf
SHELL		=	/usr/bin/bash

MAKE_DB		=	./.make_db
MAKE_EXEC	=	$(MAKE_DB)/.executables_ok
MAKE_ERROR_LOG						\
			=	$(MAKE_DB)/error.log

EXECUTABLES	=	./tests/42tester \
				./tests/*.sh \
				./tests/*.py \
				./www/cgi-bin/*.sh \
				./www/cgi-bin/*.py \
				./www/cgi-bin/*.php \
				./www_42tester/cgi-bin/cgi_tester \
				./www_42tester/cgi-bin/*.sh \
				./www_42tester/cgi-bin/*.py \
				./www_42tester/cgi-bin/*.php

INC_DIR		=	./inc
SRC_DIR		=	./src
OBJ_DIR		=	./obj
BIN_DIR		=	./bin

NAME		=	$(BIN_DIR)/webserv

# TO GET THE ALL SOURCE FILES (TEMPORARILY), UNTIL THE PROJECT BECOMES MORE STABLE
TMP_SRC_DIRS	= $(shell find $(SRC_DIR) -type d)
TMP_OBJ_DIRS	= $(subst $(SRC_DIR),$(OBJ_DIR),$(TMP_SRC_DIRS))
TMP_SRC_FILES	= $(wildcard $(addsuffix /*, $(TMP_SRC_DIRS)))
INC_FILES		= $(shell find $(INC_DIR) -name "*.hpp")
SRC_FILES		= $(filter %.cpp, $(TMP_SRC_FILES))

# COMMENTED UNTIL CLOSE TO THE FINAL STAGES
SRC_DIRS	=	$(TMP_SRC_DIRS)
OBJ_DIRS	=	$(TMP_OBJ_DIRS)
# INC_FILES	=	$(INC_DIR)/WebServ.hpp

# SRC_FILES	=	$(SRC_DIR)/WebServ.cpp \
				$(SRC_DIR)/main.cpp

OBJ_FILES	=	$(subst $(SRC_DIR)/,$(OBJ_DIR)/,$(SRC_FILES:%.cpp=%.o))

GRAY		:=	\001\033[0;37m\002
CYAN		:=	\001\033[36m\002
GREEN		:=	\001\033[32m\002
RED			:=	\001\033[1;31m\002
END			:=	\001\033[0m\002
CLEAR		:=	\001\033[K\002

# $1 is the action verb that this process is all about; ex: Compiling
# $2 is the action verb in case of success or failure; ex: compiled
# $3 is the targeted file name
# $4 is the command that will be executed to get the target file
define anim
	@echo -ne "\r$(GRAY)$1 $(END)$3$(GRAY)...$(CLEAR)"
	@$4 > $(MAKE_ERROR_LOG) 2>&1 && \
	echo -e "\r$(GREEN)OK: $(END)$3 $(GRAY)$2.$(CLEAR)" || \
	(echo -e "\r$(RED)ERROR: $(GRAY)$1 failed for$(END) $3$(CLEAR)" && cat $(MAKE_ERROR_LOG) && rm -f $(MAKE_ERROR_LOG) && exit 1)
	@rm -f $(MAKE_ERROR_LOG)
endef

all: $(MAKE_DB) $(NAME)

$(NAME): $(INC_FILES) $(OBJ_FILES) | $(BIN_DIR)
	@echo
	$(call anim,Linking,linked,$(NAME),$(CC) $(CFLAGS) $(DFLAGS) -I$(INC_DIR) $(OBJ_FILES) -o $@)

$(NAME): | set_executables www/delete_test

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(INC_FILES) | $(OBJ_DIR)
	$(call anim,Compiling,compiled,$@,$(CC) $(CFLAGS) $(DFLAGS) -I$(INC_DIR) -c $< -o $@)

$(OBJ_DIR): EXTRA_DIRS := $(OBJ_DIRS)

$(OBJ_DIR) $(BIN_DIR) $(MAKE_DB):
	@echo -en "$(CYAN)Creating$(GRAY) $(@)..."
	@mkdir -p $@ $(EXTRA_DIRS)
	@echo -e "\r$(CLEAR)$(GREEN)OK:$(END) $(@) $(GRAY)created!$(END)"

www/delete_test:
	$(call anim,Creating,created,$@,./tests/prep_www_for_delete_testing.sh)

test:
	@echo SRC_DIRS:
	@echo $(SRC_DIRS)
	@echo SRC_FILES:
	@echo $(SRC_FILES)
	@echo OBJ_DIRS:
	@echo $(OBJ_DIRS)
	@echo OBJ_FILES:
	@echo $(OBJ_FILES)

runtests:
	@/bin/bash tests/run_python_tests.sh

set_executables: | $(MAKE_EXEC)

$(MAKE_EXEC): | $(MAKE_DB)
	$(call anim,Setting permissions,permissions set,$@,chmod +x $(EXECUTABLES) 2>/dev/null || true)
	@touch $@

clean:
	$(call anim,Removing,removed,$(OBJ_DIR),@$(RM) $(OBJ_DIR))

fclean: clean
	$(call anim,Removing,removed,$@,@$(RM) $(BIN_DIR))
	$(call anim,Removing,removed,./www/delete_test,./tests/prep_www_for_delete_testing.sh fclean)
	@$(RM) $(MAKE_DB)

re: fclean all

.PHONY: all clean fclean re \
		test \
		runtests \
		set_executables