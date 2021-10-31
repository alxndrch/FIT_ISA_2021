/**
 * @brief Implementace projektu do ISA
 * @file isa.cpp
 *
 * @author Alexandr Chalupnik <xchalu15@stud.fit.vutbr.cz>
 * @date 28.10 2021
 */

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <getopt.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "isa.h"

using namespace std;

// https://nachtimwald.com/2017/11/18/base64-encode-and-decode-in-c/
// https://stackoverflow.com/a/41094722

static const unsigned char base64_table[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


static const int B64index[256] = 
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 62, 63, 62, 62, 63, 52, 53, 54, 55,
      56, 57, 58, 59, 60, 61,  0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,
      7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,  0,
      0,  0,  0, 63,  0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
      41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51 };

string base64_encode(const unsigned char *src, size_t len)
{
    unsigned char *out, *pos;
    const unsigned char *end, *in;

    size_t olen;

    olen = 4*((len + 2) / 3); /* 3-byte blocks to 4-byte */

    if (olen < len)
        return string(); /* integer overflow */

    string outStr;
    outStr.resize(olen);
    out = (unsigned char*)&outStr[0];

    end = src + len;
    in = src;
    pos = out;
    while (end - in >= 3) {
        *pos++ = base64_table[in[0] >> 2];
        *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
        *pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
        *pos++ = base64_table[in[2] & 0x3f];
        in += 3;
    }

    if (end - in) {
        *pos++ = base64_table[in[0] >> 2];
        if (end - in == 1) {
            *pos++ = base64_table[(in[0] & 0x03) << 4];
            *pos++ = '=';
        }
        else {
            *pos++ = base64_table[((in[0] & 0x03) << 4) |
                (in[1] >> 4)];
            *pos++ = base64_table[(in[1] & 0x0f) << 2];
        }
        *pos++ = '=';
    }

    return outStr;
}

string b64decode(const void* data, const size_t len)
{
    unsigned char* p = (unsigned char*)data;
    int pad = len > 0 && (len % 4 || p[len - 1] == '=');
    const size_t L = ((len + 3) / 4 - pad) * 4;
    std::string str(L / 4 * 3 + pad, '\0');

    for (size_t i = 0, j = 0; i < L; i += 4)
    {
        int n = B64index[p[i]] << 18 | B64index[p[i + 1]] << 12 | B64index[p[i + 2]] << 6 | B64index[p[i + 3]];
        str[j++] = n >> 16;
        str[j++] = n >> 8 & 0xFF;
        str[j++] = n & 0xFF;
    }
    if (pad)
    {
        int n = B64index[p[L]] << 18 | B64index[p[L + 1]] << 12;
        str[str.size() - 1] = n >> 16;

        if (len > L + 2 && p[L + 2] != '=')
        {
            n |= B64index[p[L + 2]] << 6;
            str.push_back(n >> 8 & 0xFF);
        }
    }
    return str;
}

int main(int argc, char *argv[])
{
    Params par = {.addr = nullptr, .port = nullptr, .cmd = CMD_INVALID, .help = false};
    int command_index = 0;

    int retval = arg_process(argc, argv, par);

    if (retval == ERR)
    // chyba pri parsovani argumentu
        return EXIT_FAILURE;

    if(par.help)
    // vypise se help a program se ukonci
        return EXIT_SUCCESS;

    if (retval != SUCC){
    // retval != SUCC a retval != ERR, tzn. retval je pozice mozneho prikazu
        command_index = retval;
        if (cmd_process(argc, argv, command_index, par) == ERR)
        // chybne zadany prikaz, nebo neplatny pocet argumentu
            return EXIT_FAILURE;
    }

    // implicitni nastaveni
    if (par.addr == nullptr) par.addr = (char *)"localhost";
    if (par.port == nullptr) par.port = (char *)"32323";

    if (connect_to_server(par, cmd_args_num(par.cmd) ? &argv[command_index + 1] : nullptr) == ERR)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

int arg_process(int argc, char** argv, Params &params)
{
    int opt = 0;
    int prev_opt = opt;
    int option_index = -1;
    opterr = 0;

    struct option long_opt[] = {
        {"address", required_argument, nullptr,'a'},
        {"port", required_argument, nullptr,'p'},
        {"help", no_argument, nullptr,'h'},
        {0, 0, 0, 0}
    };

    if (argc < 2){
        cerr << "client: expects <command> [<args>] ... on the command line, given 0 arguments\n";
        return ERR;
    }

    while (1){
        if ((opt = getopt_long(argc, argv, "+:a:p:h", long_opt, nullptr)) == -1)
            break;

        switch(opt){
            case 'a':
                if (params.addr != nullptr){
                    cerr << "client: only one instance of one option from (-a --address) is allowed\n";
                    return ERR;
                }
                params.addr = optarg;
                break;
            case 'p':
                if (params.port != nullptr){
                    cerr << "client: only one instance of one option from (-p --port) is allowed\n";
                    return ERR;
                }
                params.port = optarg;
                break;
            case 'h':
                params.help = true;
                cout << "usage: client [ <option> ... ] <command> [<args>] ...\n\n";
                cout << "<option> is one of\n\n";
                cout << "   -a <addr>, --address <addr>\n";
                cout << "     Server hostname or address to connect to\n";
                cout << "   -p <port>, --port <port>\n";
                cout << "     Server port to connect to\n";
                cout << "   --help, -h\n";
                cout << "     Show this help\n";
                cout << "   --\n";
                cout << "     Do not treat any remaining argument as a switch (at this level)\n\n";
                cout << " Multiple single-letter switches can be combined after\n";
                cout << " one `-`. For example, `-h-` is the same as `-h --`.\n";
                cout << " Supported commands:\n";
                cout << "   register <username> <password>\n";
                cout << "   login <username> <password>\n";
                cout << "   list\n";
                cout << "   send <recipient> <subject> <body>\n";
                cout << "   fetch <id>\n";
                cout << "   logout\n";

                return SUCC;
            case ':':
                cout << "client: the \"" << argv[optind-1] << "\" option needs 1 argument, but 0 provided\n";
                return ERR;
            case '?':
                cerr << "client: unknown switch: " << argv[optind-1] << endl;
                return ERR;
        }
    }

    if (optind < argc)
        return optind;
    else if (optind == argc){
        cerr << "client: expects <command> [<args>] ... on the command line, given 0 arguments\n";
        return ERR;
    }

    return SUCC;
}

int cmd_process(int argc, char** argv, int cmd_pos, Params &params)
{
    int cmd = CMD_INVALID;
    if ((cmd = is_command(argv[cmd_pos])) != CMD_INVALID){
        int args_num = cmd_args_num(cmd);

        if (args_num != (argc - cmd_pos - 1)){
                if (cmd == CMD_REGISTER)
                    cerr << "register <username> <password>" << endl;
                else if (cmd == CMD_LOGIN)
                    cerr << "login <username> <password>" << endl;
                else if (cmd == CMD_LIST)
                    cerr << "list" << endl;
                else if (cmd == CMD_SEND)
                    cerr << "send <recipient> <subject> <body>" << endl;
                else if (cmd == CMD_FETCH)
                    cerr << "fetch <id>" << endl;
                else if (cmd == CMD_LOGOUT)
                    cerr << "logout" << endl;
            return ERR;
        }
    }else{
        cerr << "unknown command" << endl;
        return ERR;
    }

    params.cmd = cmd;
    return SUCC;
}

int is_command(char *arg)
{
    string check(arg);

    if (check == "register")
        return CMD_REGISTER;
    else if (check == "login")
        return CMD_LOGIN;
    else if (check == "list")
        return CMD_LIST;
    else if (check == "send")
        return CMD_SEND;
    else if (check == "fetch")
        return CMD_FETCH;
    else if (check == "logout")
        return CMD_LOGOUT;

    return CMD_INVALID;
}

uint cmd_args_num(int cmd)
{
    if (cmd == CMD_REGISTER|| cmd == CMD_LOGIN )
        return CMD_ARGC_2;
    else if (cmd == CMD_SEND)
        return CMD_ARGC_3;
    else if (cmd == CMD_FETCH)
        return CMD_ARGC_1;
    else // list, logout
        return CMD_ARGC_0;
}

int connect_to_server(Params &params, char **args)
{
    int sd;
    int port = 0;
    int err = str2int(params.port, port);

    if (err == ERR && port == 0){
        cerr << "Port number is not a string\n";
        return ERR;
    }else if ((errno == ERANGE && (err == LONG_MIN || err == LONG_MAX )) 
             || port < PORT_MIN || port > PORT_MAX){
        cerr << "tcp-connect: contract violation\n";
        cerr << "  expected: port-number?\n";
        cerr << "  given: " << params.port << endl;
        return ERR;
    }
    
    struct addrinfo server;
    struct addrinfo *l_list;

    memset(&server, 0, sizeof(server));
    server.ai_family = AF_UNSPEC;
    server.ai_socktype = SOCK_STREAM;

    if ((err = getaddrinfo(params.addr, params.port, &server, &l_list)) != 0) {
        cerr << "tcp-connect: host not found\n";
        cerr << "  hostname: " << params.addr << endl;
        cerr << "  port number: " << params.port << endl;
        cerr << "  system error: " << gai_strerror(err) << "; gai_err=" << err << endl;
        return ERR;
    }
    
    for (struct addrinfo *ip = l_list; ip != nullptr; ip = ip->ai_next) {
        sd = socket(ip->ai_family, SOCK_STREAM, 0);
        if (sd < 0){
            if (ip->ai_next == nullptr){
                cerr << "Unable to create socket\n";
                freeaddrinfo(l_list);
                close(sd);
                return ERR;
            }
            continue;
        }

        if (connect(sd, ip->ai_addr, ip->ai_addrlen) == 0)
            break;
        else if (ip->ai_next == nullptr){
            cerr << "tcp-connect: connection failed\n";
            cerr << "  hostname: " << params.addr << endl;
            cerr << "  port number: " << port << endl;
            cerr << "  system error: " << strerror(errno) << "; errno=" << errno << endl;
            freeaddrinfo(l_list);
            close(sd);
            return ERR;
        }                

        close(sd);
    }

    if(send_message(sd, params.cmd, args) == ERR){
        close(sd);
        return ERR;
    }

    if(recv_message(sd, params.cmd) == ERR){
        close(sd);
        return ERR;
    }

    close(sd);

    return SUCC;
}

int send_message(int sockfd, int cmd, char **args)
{
    char send_str[MSG_MAX_LEN];
    memset(send_str, '\0', MSG_MAX_LEN);

    build_request(cmd, args, send_str);

    cout << "builded: " << send_str << endl;

    /*
    if (cmd == CMD_REGISTER){
        int total_written = 0;
        int index = 0;
        int written = 0;

        char msg[MSG_MAX_LEN] = "(register \"user\" \"dXNlcg==\")\0";
        
        while (total_written < strlen(msg)){
            int written = send(sockfd, &msg[index], 5, 0);

            if (written == -1)
            {
                cerr << "error" << endl;
                break;
            }
            index += written;
            total_written += written;
            cout << msg << ", " << strlen(msg) << ", " << index << endl;
        }
    }*/

    return SUCC;
}

int recv_message(int sockfd, int cmd)
{
    char recv_str[MSG_MAX_LEN];
    memset(recv_str, '\0', MSG_MAX_LEN);

    /*
    if(recv(sd, server_message, sizeof(server_message), 0) < 0){
        cerr << "Unable to receive message\n";
        return ERR;
    }
    */
    return SUCC;
}

int build_request(int cmd, char **args, char *request)
{
    if (cmd == CMD_REGISTER){
        cout << args[0] << endl;
        cout << args[1] << endl;

    }else if (cmd == CMD_LOGIN){

    }else if (cmd == CMD_SEND){

    }else if (cmd == CMD_FETCH){

    }else if (cmd == CMD_LIST){

    }else{ // logout

    }

    return SUCC;
}

int parse_response(int cmd, char *response)
{
    if (cmd == CMD_REGISTER){

    }else if (cmd == CMD_LOGIN){

    }else if (cmd == CMD_SEND){

    }else if (cmd == CMD_FETCH){

    }else if (cmd == CMD_LIST){

    }else{ // logout

    }

    return SUCC;
}

int str2int(char* str, int &num)
{
    errno = 0;
    char* end;

    num = (int)strtol(str, &end, 10);

    if (((errno == ERANGE || errno == EINVAL || errno != 0) && num == 0) || *end)
        return ERR;

    return SUCC;
}