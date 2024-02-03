CC = c++
CPPFLAGS = -Wall -Wextra -Werror -std=c++98 #-g3 -fsanitize=address 
SRC = main.cpp src/Multiplexer/Server.cpp src/Multiplexer/Location.cpp \
		src/Multiplexer/Webserver.cpp src/Multiplexer/Utils.cpp  src/Multiplexer/Config.cpp \
		src/Request/Request.cpp src/Request/Helpers.cpp src/Request/Boundaries.cpp src/Request/Chunks.cpp \
		src/Response/Response.cpp \
		
OBJ = $(SRC:.cpp=.o)
NAME = webserv

%.o: %.cpp 
	$(CC) $(CPPFLAGS) -c $< -o $@
all:$(NAME)
	
run:all
	@./$(NAME)
	@make -s fclean
testing:all
	@./$(NAME) conf/default.conf
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