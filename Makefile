# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: aistok <aistok@student.42london.com>       +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2026/01/14 18:42:19 by aistok            #+#    #+#              #
#    Updated: 2026/06/06 23:15:45 by aistok           ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

CC			:=	c++

CFLAGS		:=	-Wall -Werror -Wextra -std=c++98 -pedantic
CFLAGS		:=	$(CFLAGS) @clang++includes.txt
#				in order for clang++ to be able to locate required includes
#				like <iostream> etc @clang++includes.txt is needed
CFLAGS		:=	$(CFLAGS) -MMD -MP # used for automatic dependency checking

#DFLAGS		:=	-g3 -O0
#DFLAGS		:=	-fsanitise=address

RM			:=	rm -rf
SHELL		:=	/usr/bin/bash

MAKE_DB		:=	./.make_db
MAKE_EXEC	:=	$(MAKE_DB)/.executables_ok
MAKE_SNAKE	:=	$(MAKE_DB)/.snake_php_cloned
MAKE_ERROR_LOG						\
			:=	$(MAKE_DB)/error.log

EXECUTABLES	:=	./tests/42tester \
				./tests/*.sh \
				./tests/*.py \
				./www_42tester/cgi-bin/cgi_tester

INC_DIR		:=	./inc
SRC_DIR		:=	./src
OBJ_DIR		:=	./obj
BIN_DIR		:=	./bin

NAME		:=	$(BIN_DIR)/webserv

# TO GET THE ALL SOURCE FILES (TEMPORARILY), UNTIL THE PROJECT BECOMES MORE STABLE
#TMP_SRC_DIRS	:= $(shell find $(SRC_DIR) -type d)
#TMP_OBJ_DIRS	:= $(subst $(SRC_DIR),$(OBJ_DIR),$(TMP_SRC_DIRS))
#TMP_SRC_FILES	:= $(wildcard $(addsuffix /*, $(TMP_SRC_DIRS)))
#INC_FILES		:= $(shell find $(INC_DIR) -name "*.hpp")
#SRC_FILES		:= $(filter %.cpp, $(TMP_SRC_FILES))

# COMMENTED UNTIL CLOSE TO THE FINAL STAGES
#SRC_DIRS	:=	$(TMP_SRC_DIRS)
#OBJ_DIRS	:=	$(TMP_OBJ_DIRS)
# INC_FILES	:=	$(INC_DIR)/WebServ.hpp

SRC_DIRS	:=	$(SRC_DIR) $(SRC_DIR)/HTTP
OBJ_DIRS	:=	$(subst $(SRC_DIR),$(OBJ_DIR),$(SRC_DIRS))
INC_DIRS	:=	$(subst $(SRC_DIR),$(INC_DIR),$(SRC_DIRS))
INC_DIRS_FLAG \
			:=	$(addprefix -I,$(INC_DIRS))

SRC_FILES	:=	$(SRC_DIR)/CGI.cpp \
				$(SRC_DIR)/Config.cpp \
				$(SRC_DIR)/Connection.cpp \
				$(SRC_DIR)/DirectoriesToHTML.cpp \
				$(SRC_DIR)/ErrorPages.cpp \
				$(SRC_DIR)/Listener.cpp \
				$(SRC_DIR)/MockTestFunctions.cpp \
				$(SRC_DIR)/Utils.cpp \
				$(SRC_DIR)/WebServ.cpp \
				$(SRC_DIR)/HTTP/HTTP_FieldName.cpp \
				$(SRC_DIR)/HTTP/HTTP_Method.cpp \
				$(SRC_DIR)/HTTP/HTTP_Request.cpp \
				$(SRC_DIR)/HTTP/HTTP_ResponseBuilder.cpp \
				$(SRC_DIR)/HTTP/HTTP_Response.cpp \
				$(SRC_DIR)/HTTP/HTTP_Status.cpp \
				$(SRC_DIR)/HTTP/HTTP_Version.cpp \
				$(SRC_DIR)/main.cpp

OBJ_FILES	:=	$(subst $(SRC_DIR)/,$(OBJ_DIR)/,$(SRC_FILES:%.cpp=%.o))

DELETE_TEST_DIR \
			:=	www/delete_test

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
define fancylog
	echo -ne "\r$(GRAY)$1 $(END)$3$(GRAY)...$(CLEAR)"; \
	$4 > $(MAKE_ERROR_LOG) 2>&1 && \
	echo -e "\r$(GREEN)OK: $(END)$3 $(GRAY)$2.$(CLEAR)" || \
	(echo -e "\r$(RED)ERROR: $(GRAY)$1 failed for$(END) $3$(CLEAR)" && cat $(MAKE_ERROR_LOG) && rm -f $(MAKE_ERROR_LOG) && exit 1); \
	rm -f $(MAKE_ERROR_LOG)
endef

all: $(MAKE_DB) $(NAME)

$(NAME): $(INC_FILES) $(OBJ_FILES) | $(BIN_DIR)
	@echo
	@$(call fancylog,Linking,linked,$(NAME),$(CC) $(CFLAGS) $(DFLAGS) $(INC_DIRS_FLAG) $(OBJ_FILES) -o $@)

$(NAME): | set_executables $(DELETE_TEST_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(INC_FILES) | $(OBJ_DIR)
	@$(call fancylog,Compiling,compiled,$@,$(CC) $(CFLAGS) $(DFLAGS) $(INC_DIRS_FLAG) -c $< -o $@)

$(OBJ_DIR): EXTRA_DIRS := $(OBJ_DIRS)
$(OBJ_DIR) $(BIN_DIR) $(MAKE_DB):
	@echo -en "$(CYAN)Creating$(GRAY) $(@)..."
	@mkdir -p $@ $(EXTRA_DIRS)
	@echo -e "\r$(CLEAR)$(GREEN)OK:$(END) $(@) $(GRAY)created!$(END)"

$(DELETE_TEST_DIR): | $(MAKE_DB)
	@$(call fancylog,Creating,created,$@,./tests/prep_www_for_delete_testing.sh)

# this is for Makefile debug only
test:
	@echo CFLAGS:
	@echo $(CFLAGS)
	@echo SRC_DIRS:
	@echo $(SRC_DIRS)
	@echo SRC_FILES:
	@echo $(SRC_FILES)
	@echo OBJ_DIRS:
	@echo $(OBJ_DIRS)
	@echo OBJ_FILES:
	@echo $(OBJ_FILES)
	@echo INC_DIRS:
	@echo $(INC_DIRS)
	@echo INC_DIRS_FLAGS:
	@echo $(INC_DIRS_FLAGS)

run: all
	@echo
	@echo "    NOTE: you may open a web browser and navigate to http://localhost:8080"
	@echo "          or you may use curl or wget or other tools to send requests to"
	@echo "          this web server"
	@echo
	@$(NAME)

42config: all
	@echo
	@echo "    NOTE: in another terminal, please run"
	@echo "          make 42tester"
	@echo
	@$(NAME) config/42tester.conf

42tester: all
	@./tests/42tester http://localhost:8080

runtests core_tests: all
	@/bin/bash tests/run_python_tests.sh

$(MAKE_SNAKE):
	@git clone https://github.com/grisendo/snake-game-php www/snake
	@touch $@

snake_php: all $(MAKE_SNAKE)
	@echo
	@echo "    SNAKE-GAME-PHP is now installed"
	@echo
	@echo "    If webserv is running, please navigate to:"
	@echo "    http://localhost:8080/snake/index.php"
	@echo
	@echo "    Controls (in FireFox & Chrome):"
	@echo "    alt + shift + {i,k,j,l}"
	@echo

clean_snake:
	@if [ -e $(MAKE_SNAKE) ]; then \
		$(RM) $(MAKE_SNAKE); \
		$(RM) www/snake; \
		$(call fancylog,Removing,removed,snake_php,:); \
	fi

set_executables: | $(MAKE_EXEC)

$(MAKE_EXEC): EXECUTABLES_SUBSTR := $(shell EXECUTABLES="$(EXECUTABLES)"; echo "$${EXECUTABLES:0:50} ...")
$(MAKE_EXEC): | $(MAKE_DB)
	@$(call fancylog,Setting permissions,permissions set,$(EXECUTABLES_SUBSTR),chmod +x $(EXECUTABLES) 2>/dev/null || true)
	@touch $@

clean:
	@if [ -d $(OBJ_DIR) ]; then \
		$(call fancylog,Removing,removed,$(OBJ_DIR),$(RM) $(OBJ_DIR) || true); \
	fi

fclean: clean clean_snake
	@if [ -d $(BIN_DIR) ]; then \
		$(call fancylog,Removing,removed,$(BIN_DIR),$(RM) $(BIN_DIR) || true); \
	fi
	@if [ -d $(DELETE_TEST_DIR) ]; then \
		$(call fancylog,Removing,removed,$(DELETE_TEST_DIR),./tests/prep_www_for_delete_testing.sh fclean); \
	fi
	@$(RM) $(MAKE_DB)

config_tester:
	python3 tests/config_tester.py

siege-basic: all
	siege -c10 -t1m http://localhost:8080

siege-medium: all
	siege -c50 -t3m -b -f ./tests/siege/www-urls.txt

siege-hard: all
	siege -c255 -r10 -b -f ./tests/siege/www-urls.txt

siege-very-hard: all
	siege -c255 -r1000 -b -f ./tests/siege/www-urls.txt

re: fclean all

.PHONY: all clean fclean re \
		test \
		runtests core_tests \
		set_executables \
		run \
		42config 42tester \
		config_tester \
		siege-basic siege-medium siege-hard siege-very-hard \
		snake_php clean_snake
