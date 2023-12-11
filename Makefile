CC = c++
CPPFLAGS = -Wall -Wextra -Werror -std=c++98 -fsanitize=address -g3
SRC = main.cpp src/Server.cpp src/Location.cpp  src/Utils.cpp  src/Config.cpp \
		src/Request/Request.cpp
OBJ = $(SRC:.cpp=.o)
NAME = webserv

%.o: %.cpp 
	$(CC) $(CPPFLAGS) -c $< -o $@
all:$(NAME)
	
run:all
	@./$(NAME)
	@make -s fclean
testing:all
	@./$(NAME) /conf/test.conf
	@make -s fclean
$(NAME):$(OBJ)
	$(CC) $(CPPFLAGS) $(OBJ) -o $(NAME)
clean:
	@$(RM) $(OBJ)
fclean:clean
	@$(RM) $(NAME)
re:fclean all
leaks:all
	@valgrind --leak-check=full --show-leak-kinds=all ./$(NAME) 2>&1 | grep total