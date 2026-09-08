CC = c++
CPPFLAGS = -Wall -Wextra -Werror -std=c++98
SRC = main.cpp src/Server/Server.cpp src/Server/Location.cpp \
		src/Server/Webserver.cpp src/Server/ConfigUtils.cpp  src/Server/Config.cpp \
		src/Request/Request.cpp src/Request/Helpers.cpp src/Request/Boundaries.cpp src/Request/Chunks.cpp \
		src/Response/Response.cpp src/Response/Cgi.cpp \

OBJ = $(SRC:.cpp=.o)
NAME = webserv

%.o: %.cpp
	$(CC) $(CPPFLAGS) -c $< -o $@

.PHONY: all run clean fclean re leaks test

all:$(NAME)
	
run:all
	@./$(NAME)
	@make -s fclean
$(NAME):$(OBJ)
	$(CC) $(CPPFLAGS) $(OBJ) -o $(NAME)
clean:
	@$(RM) $(OBJ)
fclean:clean
	@$(RM) $(NAME)
re:fclean all
leaks:all
	valgrind --leak-check=full --show-leak-kinds=all ./$(NAME) 2>&1 | grep total
test:
	@./tests/run_tests.sh