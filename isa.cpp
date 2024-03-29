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
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <regex>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "isa.h"
#include "base64.cpp"  // base64_encode

using namespace std;

int main(int argc, char *argv[])
{
    Params par = {
        nullptr, // addr
        nullptr, // port
        CMD_INVALID, // cmd
        false // help
    };
    int command_index = 0; // index prikazu v argv

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
    /* Funkce zpracuje argumenty zadane na prikazove radce.
     * kontroluje se pouze spravnost parametru address a port,
     * pri chybe se vraci chybova hlaska na stderr a navratova
     * hodnota ERR. Vysledne parametry se ulozi do struktury
     * Params. Pokud nebyl jeden z parametru zadan, jeho hodnota 
     * bude nullptr.
     */
    int opt = 0;
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
    /* Funkce zkontrolu spravost zadaneho prikazu vcetne poctu
     * zadanych parametru v argv. cmd_pos obshuje index na prikaz.
     * Pokud jde o neplatny prikaz, spaty pocet parametru, vypisuje
     * chybova hlaska na stderr a vraci se ERR, jinak SUCC.
     * Typ prikazu se uklada do params.cmd jako konstanta
     */
    int cmd = CMD_INVALID;
    if ((cmd = is_command(argv[cmd_pos])) != CMD_INVALID){
        int args_num = cmd_args_num(cmd);

        if (args_num != (argc - cmd_pos - 1)){
            // neplatny pocet argumentu u zadaneho prikazu
                if (cmd == CMD_REGISTER)
                    cerr << "register <username> <password>" << endl;
                else if (cmd == CMD_LOGIN)
                    cerr << "login <username> <password>" << endl;
                else if (cmd == CMD_LIST)
                    cerr << "list " << endl;
                else if (cmd == CMD_SEND)
                    cerr << "send <recipient> <subject> <body>" << endl;
                else if (cmd == CMD_FETCH)
                    cerr << "fetch <id>" << endl;
                else if (cmd == CMD_LOGOUT)
                    cerr << "logout " << endl;
            return ERR;
        }
    }else{
        // neplatny prikaz
        cerr << "unknown command" << endl;
        return ERR;
    }

    params.cmd = cmd;
    return SUCC;
}

int is_command(char *arg)
{
    /* Predikat a zaroven kontrola prikazu. Pokud jdo o neplatny
     * prikaz v arg, vraci se CMD_INVALID, jinak se vraci
     * konstanta pro dany prikaz.
     */
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
    /* Funkce vraci pocet parametru pro prikaz cmd
     */
    if (cmd == CMD_REGISTER|| cmd == CMD_LOGIN)
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
    /* Funkce se pro pripojeni k zadanemu serveru,
     * a naslednou komunikaci.
     * V pripade chyby se vraci hlaska na stderr
     * a navratova hodnota ERR, jinak SUCC.
     */
    int sd;
    int port = 0;
    int err = str2int(params.port, port);

    if (err == ERR && port == 0){
        // neplatna hodnota portu
        cerr << "Port number is not a string\n";
        return ERR;
    }else if ((errno == ERANGE) || port < PORT_MIN || port > PORT_MAX){
        // hodnota portu je mimo interval
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
        // neplatna hodnota adresy
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
            // chyba pri pripojeni k serveru
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
    // vytvorani a odeslani pozadavku
    if(send_message(sd, params.cmd, args) == ERR){
        close(sd);
        return ERR;
    }
    // prijeti a vypis odpovedi
    if(recv_message(sd, params.cmd) == ERR){
        close(sd);
        return ERR;
    }

    close(sd);

    return SUCC;
}

int send_message(int sockfd, int cmd, char **args)
{
    /* Funkce zajistit sestaveni pozadavku a jeho odeslani.
     * Args obsahuje pozadovane parametry prikazu.
     * V pripade chyby se vraci hlaska na stderr
     * a navratova hodnota ERR, jinak SUCC.
     */
    char send_str[MSG_MAX_LEN];
    memset(send_str, '\0', MSG_MAX_LEN);

    // vytvoreni pozadavku podle formatu protoklu
    if (build_request(cmd, args, send_str) == ERR)
        return ERR;

    int total_written = 0;
    while (total_written < strlen(send_str)){
        int written = send(sockfd, &send_str[total_written],
                           strlen(send_str) - total_written, 0);

        if (written == -1){
            // chyba pri odesilani zpravy
            cerr << "Error: " << strerror(errno) << "; errno=" << errno << endl;
            break;
        }
        total_written += written;
    }

    return SUCC;
}

int recv_message(int sockfd, int cmd)
{
    /* Funkce zajistit prijeti a vypis zpravy.
     * V pripade chyby se vraci hlaska na stderr
     * a navratova hodnota ERR, jinak SUCC.
     */
    char recv_str[MSG_MAX_LEN];
    int received_len = 0;
    memset(recv_str, '\0', MSG_MAX_LEN);

    if((received_len = recv(sockfd, recv_str, MSG_MAX_LEN, MSG_WAITALL)) < 0){
        // chyba pri prijeti odpovedi
        cerr << "Error: " << strerror(errno) << "; errno=" << errno << endl;
        return ERR;
    }

    unescape_backslash(recv_str, received_len);

    // vypis odpovedi
    if (parse_response(cmd, recv_str, received_len) == ERR)
        return ERR;

    return SUCC;
}

int build_request(int cmd, char **args, char *request)
{
    /* Funkce sestavi pozadavek odesilany serveru, ze zadanych
     * parametru v args. Tvar zpravy odpovida formatu prokolu.
     * Zpravu propagujeme pres ukazatel request
     */
    string logged_user; // login-token prihlaseneho uzivatele

    if (cmd == CMD_SEND || cmd == CMD_FETCH
        || cmd == CMD_LIST || cmd == CMD_LOGOUT){
        // zjisteni login tokenu prihlaseneho uzivatele
        ifstream login_token("login-token");
        if(!login_token.is_open()){
            cerr << "Not logged in\n";
            return ERR;
        }
        getline(login_token, logged_user);
        login_token.close();
    }
    // vytvareni pozadavku
    if (cmd == CMD_REGISTER){
        sprintf(request, "(register \"%s\" \"%s\")", args[0],
                    base64_encode((const unsigned char *)args[1],
                    strlen(args[1])).c_str());
    }else if (cmd == CMD_LOGIN){
        sprintf(request, "(login \"%s\" \"%s\")", args[0],
            base64_encode((const unsigned char *)args[1],
            strlen(args[1])).c_str());
    }else if (cmd == CMD_SEND){
        sprintf(request, "(send %s \"%s\" \"%s\" \"%s\")",
                                logged_user.c_str(), args[0],
                                args[1], args[2]);
    }else if (cmd == CMD_FETCH){
        int fetch_id = 0;
        int err = str2int(args[0], fetch_id);
        if (err == ERR && fetch_id == 0 && errno != ERANGE){
            // neplate id zadane pro FETCH
            cerr << "ERROR: id " << args[0] <<" is not a number\n";
            return ERR;
        }
        sprintf(request, "(fetch %s %s)",
                                logged_user.c_str(), args[0]);
    }else if (cmd == CMD_LIST){
        sprintf(request, "(list %s)",
                                logged_user.c_str());
    }else{ // logout
        sprintf(request, "(logout %s)",
                                logged_user.c_str());

        if (access("login-token", F_OK) != -1){
            remove("login-token");
        }
    }

    if (escape_backslash(request, strlen(request)) == ERR){
        cerr << "ERROR: Request is too long" << endl;
        return ERR;
    }

    return SUCC;
}

int parse_response(int cmd, char *response, int response_len)
{
    /* Funce parsuje a vypisuje prijatou odpoved.
     */
    int status_skip = MSG_ERR_STATUS_LEN;

    if(strncmp(response, "(ok ", 4) == 0){
        cout << "SUCCESS:";
        status_skip = MSG_SUCC_STATUS_LEN;
    }else
        cout << "ERROR:";

    if ((status_skip == MSG_ERR_STATUS_LEN)
        || (cmd != CMD_FETCH && cmd != CMD_LIST && cmd != CMD_LOGIN)){
        // kdyz jde o chybovou hlasku
        // nebo se nejdena o prikazy fetch, list, login
        response[response_len-1 - SKIP_BYTE] = '\0'; // string truncate
        cout << " " << (response + status_skip + SKIP_BYTE) << endl;
    }else{
        int c_index = status_skip + SKIP_BYTE;
        // status_skip + SKIP_BYTE preskoceni az na prvni "
        int arg = 0;
        if (cmd == CMD_FETCH){
            cout << endl << endl;

            while(arg < FETCH_ARGS){
                int msg_state = MSG_START;
                int msg_start_index = c_index;
                int msg_end_index = c_index;

                text_fsm(response, c_index, msg_end_index, msg_state);

                if(arg == 0)
                    cout << "From: ";
                else if (arg == 1)
                    cout << "Subject: ";
                else
                    cout << endl;

                response[msg_end_index] = '\0'; // string truncate
                cout << (response + msg_start_index+SKIP_BYTE);

                if (arg < 2)
                    cout << endl;

                arg++;
                c_index++;
            }

        }else if (cmd == CMD_LIST){
            if(strncmp(response, "(ok ())", 7) == 0)
                cout << endl;
            else{
                cout << endl;
                int par_cnt = 1; // pocitadlo zavorek

                while(par_cnt > 0){
                    if (response[c_index] == '(')
                        par_cnt++;
                    else if (response[c_index] == ')')
                        par_cnt--;
                    else if (response[c_index] == ' '){
                        c_index++;
                        continue;
                    }else{
                        int msg_start_index = c_index;
                        while(response[c_index] != ' '){
                            // zpracovani id
                            c_index++;
                        }
                        int msg_end_index = c_index;
                        response[msg_end_index] = '\0'; // string truncate
                        cout << (response + msg_start_index) << ":" << endl;

                        c_index++; // posun na "
                        while(arg < LIST_ARGS-1){
                            msg_start_index = c_index;
                            msg_end_index = c_index;
                            int msg_state = MSG_START;

                            text_fsm(response, c_index, msg_end_index, msg_state);

                            if(arg == 0)
                                cout << "  From: ";
                            else if (arg == 1)
                                cout << "  Subject: ";

                            response[msg_end_index] = '\0'; // string truncate
                            cout << (response + msg_start_index+SKIP_BYTE);
                            cout << endl;

                            // msg_end_index je vzdy index na koncovou uvozovku
                            // "user"
                            //      ^
                            // c_index po funckci text_fsm ma hodnotu msg_end_index + 1
                            // pokud nedoslo k precteni posledni argumentu prikazu list (subject)
                            // bude se jednat o index na ' ' za predchozi argument

                            arg++;
                            c_index++;
                            // inkrementaci hodnoty c_index se dostaneme na novou pocatecni "
                            // "user" "subject"
                            //        ^
                        }
                        arg = 0;
                        c_index-= 2;
                        // pokud dostlo k precteni vsech argumtu
                        // c_index je index na posledni zavorku
                        // "user" "subject"))
                        //                  ^
                        // musime tedy c_index odecist o 2
                        // abychom se dostali pred zavorky
                        // a snizili pocitadlo zavorek par_cnt
                    }
                    c_index++;
                }
            }

        }else if (cmd == CMD_LOGIN){
            if(strncmp(response, "(ok \"user logged in\"", 20) == 0){
                cout << " user logged in" << endl;
                int login_token_begin = 21;
                response[response_len-1] = '\0';

                ofstream login_token;
                login_token.open("login-token", fstream::in | fstream::trunc);
                //string token_to_file(response + login_token_begin);
                login_token << response + login_token_begin;

                login_token.close();
            }
        }
    }
    return SUCC;
}

int str2int(char* str, int &num)
{
    /* Prevod retezce na int
     */
    errno = 0;
    char* end;
    long ret = 0;

    ret = strtol(str, &end, 10);

    if (errno || *end != '\0' || ret < INT_MIN || ret > INT_MAX)
        return ERR;

    num = (int)ret;
    return SUCC;
}

void text_fsm(char *str, int &index, int &end_index, int state)
{
    /* Stavovy automat pro cteni podretezce mezi " ".
     * index obsahuje pocatecni pozici cteni,
     * pres end_index se vraci pozice posledni "
     */
    while(state != MSG_END){
        if (str[index] == '\"'){
            if (state == MSG_START || state == MSG_ESCAPE_SEQ){
                state = MSG_TEXT;
            }else if (state == MSG_TEXT){
                state = MSG_END;
                end_index = index;
            }
        }else if (str[index] == '\\'){
            if (state != MSG_ESCAPE_SEQ)
                state = MSG_ESCAPE_SEQ;
            else
                state = MSG_TEXT;
        }else{
            if (state == MSG_ESCAPE_SEQ){
                state = MSG_TEXT;
            }
        }
        index++;
    }
}

int escape_backslash(char *text, int len)
{
    /* Funkce zdvoji kazde \
     * pr.: novy\nradek
     *   =  novy\\nradek
     */
    int slash_needed = 0; // pocet pridavanych \/
    for(int i = 0; i < len; i++)
        if (text[i] == '\\')
            slash_needed++;

    if (slash_needed == 0)
        return SUCC;

    // pokud musime pridav vice 
    // nez mami volneho mista, vraci se err
    if (slash_needed + len+1 > MSG_MAX_LEN)
        return ERR;

    for(int i = len; i >= 0 && slash_needed != 0; i--){
        text[i+slash_needed] = text[i];
        if (text[i] == '\\'){
            slash_needed--;
            text[i+slash_needed] = text[i];
        }
    }

    return SUCC;
}

int unescape_backslash(char *text, int len)
{
    /* Funkce odstrani \, pokud jsou dve po sobe
     * odstani se pouze jedna. A vsechny podretezce
     * "\n" a "\t" zmeni na znaky '\n' '\t'
     * pr.: novy\nradek\ mezera
     *   =  novy
     *      radek mezera
     */
    for(int i = 0; i < len;i++){
        if (text[i] == '\\'){
            len--;
            memcpy(text+i, text+i+1, len-i+1);
            if (i < len){
                if (text[i] == 'n') 
                    text[i] = '\n';
                else if (text[i] == 't') 
                    text[i] = '\t';
            }
        }
    }

    return SUCC;
}
