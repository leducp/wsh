#include "linenoise.h"
#include <lua.hpp>

#include <unistd.h> 
//#include <cstdio> 
#include <cstring>
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 

#define PORT 8080 
#define LUA_MAXINPUT 512

int client_fd;


/*
** Prompt the user, read a line, and push it into the Lua stack.
*/
static int pushline (lua_State *L, int firstline) 
{
    char buffer[LUA_MAXINPUT];

    const char *prmt = "> ";
    if (not firstline)
    {
        write(client_fd, "\r\n", 2);
        prmt = ">> ";
    }

    int readstatus = linenoiseEdit(client_fd, client_fd, buffer, LUA_MAXINPUT, prmt);
    if (readstatus < 0)
    {
        return 0;  /* no input (prompt will be popped by caller) */
    }

    if (readstatus > 0 && buffer[readstatus-1] == '\n')  /* line ends with newline? */
    {
        buffer[--readstatus] = '\0';  /* remove it */
    }
    if (firstline && buffer[0] == '=')  /* for compatibility with 5.2, ... */
    {
        lua_pushfstring(L, "return %s", buffer + 1);  /* change '=' to 'return' */
    }
    else
    {
        lua_pushlstring(L, buffer, readstatus);
    }
    return 1;
}


/*
** Try to compile line on the stack as 'return <line>;'; on return, stack
** has either compiled chunk or original line (if compilation failed).
*/
static int addreturn (lua_State *L) 
{
    const char *line = lua_tostring(L, -1);  /* original line */
    const char *retline = lua_pushfstring(L, "return %s;", line);
    int status = luaL_loadbuffer(L, retline, strlen(retline), "=stdin");
    if (status == LUA_OK) 
    {
        lua_remove(L, -2);  /* remove modified line */
        if (line[0] != '\0')  /* non empty? */
        {
            linenoiseHistoryAdd(line);  /* keep history */
            linenoiseHistorySave("history.txt");
        }
    }
    else
    {
        lua_pop(L, 2);  /* pop result from 'luaL_loadbuffer' and modified line */
    }
    return status;
}


/* mark in error messages for incomplete statements */
#define EOFMARK		"<eof>"
#define marklen		(sizeof(EOFMARK)/sizeof(char) - 1)

/*
** Check whether 'status' signals a syntax error and the error
** message at the top of the stack ends with the above mark for
** incomplete statements.
*/
static int incomplete(lua_State *L, int status)
{
    if (status == LUA_ERRSYNTAX)
    {
        size_t lmsg;
        const char *msg = lua_tolstring(L, -1, &lmsg);
        if (lmsg >= marklen && strcmp(msg + lmsg - marklen, EOFMARK) == 0) 
        {
            lua_pop(L, 1);
            return 1;
        }
    }
    return 0; 
}


/*
** Read multiple lines until a complete Lua statement
*/
static int multiline (lua_State *L)
{
    for (;;) /* repeat until gets a complete statement */
    {  
        size_t len;
        const char *line = lua_tolstring(L, 1, &len);  /* gget_promptet what it has */
        printf("yolo : %s\n", line);
        int status = luaL_loadbuffer(L, line, len, "=stdin");  /* try it */
        if (not incomplete(L, status) || not pushline(L, 0)) 
        {
            linenoiseHistoryAdd(line);  /* keep history */
            linenoiseHistorySave("history.txt");
            return status;  /* cannot or should not try to add continuation line */
        }
        lua_pushliteral(L, "\r\n");  /* add newline... */
        lua_insert(L, -2);  /* ...between the two lines */
        lua_concat(L, 3);  /* join them */
    }
}


/*
** Read a line and try to load (compile) it first as an expression (by
** adding "return " in front of it) and second as a statement. Return
** the final status of load/call with the resulting function (if any)
** in the top of the stack.
*/
static int loadline (lua_State *L) 
{
    lua_settop(L, 0);
    if (not pushline(L, 1))
    {
        return -1;  /* no input */
    }

    int status = addreturn(L);
    if (status != LUA_OK)  /* 'return ...' did not work? */
    {
        status = multiline(L);  /* try as command, maybe with continuation lines */
    }
    lua_remove(L, 1);  /* remove line from the stack */
    lua_assert(lua_gettop(L) == 1);
    return status;
}


/*
** Message handler used to run all chunks
*/
static int msghandler (lua_State *L) 
{
    const char *msg = lua_tostring(L, 1);
    if (msg == nullptr)  /* is error object not a string? */
    { 
        if (luaL_callmeta(L, 1, "__tostring") and  lua_type(L, -1) == LUA_TSTRING)/* does it have a metamethod that produces a string? */
        {
            return 1;  /* that is the message */
        }
        else
        {
            msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
        }
    }
    luaL_traceback(L, L, msg, 1);  /* append a standard traceback */
    return 1;  /* return the traceback */
}


/*
** Interface to 'lua_pcall', which sets appropriate message function
** and C-signal handler. Used to run all chunks.
*/
static int docall (lua_State *L, int narg, int nres) 
{
    int base = lua_gettop(L) - narg;  /* function index */
    lua_pushcfunction(L, msghandler);  /* push message handler */
    lua_insert(L, base);  /* put it under function and args */

    int status = lua_pcall(L, narg, nres, base);

    lua_remove(L, base);  /* remove message handler from the stack */
    return status;
}


int main() 
{ 
	int server_fd, valread; 
	struct sockaddr_in address; 
	int opt = 1; 
	int addrlen = sizeof(address); 
	
	// Creating socket file descriptor 
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
	{ 
		perror("socket failed"); 
		return 1;
	} 
	
	// Forcefully attaching socket to the port 8080 
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
												&opt, sizeof(opt))) 
	{ 
		perror("setsockopt"); 
		return 1;
	} 
	address.sin_family = AF_INET; 
	address.sin_addr.s_addr = INADDR_ANY; 
	address.sin_port = htons( PORT ); 
	
	// Forcefully attaching socket to the port 8080 
	if (bind(server_fd, (struct sockaddr *)&address, 
								sizeof(address))<0) 
	{ 
		perror("bind failed"); 
		return 1;
	} 

	if (listen(server_fd, 3) < 0) 
	{ 
		perror("listen"); 
		return 1;
	} 

    // Prepare linenoise
    linenoiseSetMultiLine(1);
    linenoiseHistorySetMaxLen(500);

    //linenoiseSetCompletionCallback(completion);
    //linenoiseSetHintsCallback(hints);

    linenoiseHistoryLoad("history.txt");

    // Prepare Lua interpreter
    lua_State *L = luaL_newstate();  // create state
    if (L == nullptr)
    {
        fprintf(stderr, "Cannot create Lua state");
        return 1;
    }
    luaL_checkversion(L);
    luaL_openlibs(L);            // open standard libraries
    lua_gc(L, LUA_GCGEN, 0, 0);  // GC in generational mode

	if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) <0) 
	{ 
		perror("accept"); 
		return 1;
	}

    // Start REPL loop
    while (true) 
    {
        int status = loadline(L);
        if (status == -1)
        {
            break;
        }
        if (status == LUA_OK)
        {
            status = docall(L, 0, LUA_MULTRET);
        }

        write(client_fd, "\r\n", 2);

        if (status == LUA_OK) 
        {
            int n = lua_gettop(L);
            if (n > 0) 
            {  
                /* any result to be printed? */
                luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
                for (int i = 1; i <= n; i++) {  /* for each argument */
                    size_t l;
                    const char *s = luaL_tolstring(L, i, &l);  /* convert it to string */
                    if (i > 1)  /* not the first element? */
                    {
                        write(client_fd, "\t", 1);  /* add a tab before it */
                    }
                    write(client_fd, s, l);  /* print it */
                    lua_pop(L, 1);  /* pop result */
                }
                write(client_fd, "\r\n", 2);    
            }
        }
        else   
        {   
            size_t msg_length;
            const char *msg = lua_tolstring(L, -1, &msg_length);
            write(client_fd, msg, msg_length);
            write(client_fd, "\r\n", 2);
            
            lua_pop(L, 1);  /* remove message */
        }
    }
    lua_settop(L, 0);  /* clear stack */
    write(client_fd, "\r\n", 2);

	return 0; 
} 

