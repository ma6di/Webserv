# === Project Info ===
NAME        := webserv
CXX         := c++
CXXFLAGS    := -Wall -Wextra -Werror -std=c++98 -pedantic -g

# === Directories ===
SRC_DIRS    := . config cgi
OBJ_DIR     := obj
INC_DIRS    := config cgi utils server http

# === Sources and Objects ===
SRC_FILES   := main.cpp \
               config/Config.cpp \
               cgi/CGIHandler.cpp \
			   utils/utils.cpp \
			   server/WebServer.cpp \
			   http/Request.cpp \
			   http/Response.cpp


OBJS        := $(SRC_FILES:.cpp=.o)
OBJ_PATHS   := $(addprefix $(OBJ_DIR)/, $(OBJS))

# === Include Paths ===
INCLUDES := $(foreach dir,$(INC_DIRS),-I$(dir))

# === Build Rules ===
all: $(NAME)

$(NAME): $(OBJ_PATHS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

# === Auto Include Dependencies ===
-include $(OBJ_PATHS:.o=.d)

# === Clean Rules ===
clean:
	@rm -rf $(OBJ_DIR)

fclean: clean
	@rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
