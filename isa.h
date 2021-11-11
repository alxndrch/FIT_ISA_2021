/**
 * @brief Implementace projektu do ISA
 * @file isa.h
 *
 * @author Alexandr Chalupnik <xchalu15@stud.fit.vutbr.cz>
 * @date 28.10 2021
 */

#ifndef FIT_ISA_H
#define FIT_ISA_H

#define SUCC 0
#define ERR -1

#define PORT_MAX 65535  // maximalni hodnota portu
#define PORT_MIN 1   // minimalni hodnota portu

#define CMD_INVALID -1  // neplatny prikaz
#define CMD_REGISTER 10  // prikaz "register"
#define CMD_LOGIN 11  // prikaz "login"
#define CMD_LOGOUT 12  // prikaz "logout"
#define CMD_SEND 13  // prikaz "send"
#define CMD_FETCH 14  // prikaz "fetch"
#define CMD_LIST 15  // prikaz "list"

/* pocty argumntu u prikazu */
#define CMD_ARGC_3 3  // pro "send"
#define CMD_ARGC_2 2  // pro "login" a "reigster"
#define CMD_ARGC_1 1  // pro "fetch"
#define CMD_ARGC_0 0  // pro "logout" a "list"

/* pocet parametru pro prikazy fetch a list,
 * ktere obdrizi klient od serveru */
#define FETCH_ARGS 3  // "user" "subject" "body"
#define LIST_ARGS 3  // "id" "user" "subject"

#define MSG_MAX_LEN 32768  // maximalni delka odeslate/obdrzene zpravy

#define SKIP_BYTE 1  // posun o 1B ve zprave
#define MSG_ERR_STATUS_LEN 5  // (err_
#define MSG_SUCC_STATUS_LEN 4  // (ok_

/* konstany pro fsm */
#define MSG_TEXT 20
#define MSG_ESCAPE_SEQ 21
#define MSG_NOT_TEXT 22
#define MSG_START 23
#define MSG_END 24

/**
 * @brief parametry z prikazove radky
 */
struct Params{
    char *addr;  // adresa
    char *port;  // port
    int cmd;  // prikaz
    bool help;
};

/**
 * @brief zpracovani argmumentu
 *
 * @param argc pocet argumentu
 * @param argv argumenty
 * @param params parametry pro beh programu
 * @return int ERR v pripade chyby, SUCC - uspech, jinak pozice prikazu (command)
 */
int arg_process(int argc, char** argv, Params &params);

/**
 * @brief zpracovani prikazu
 *
 * @param argc pocet argumentu
 * @param argv argumenty
 * @param cmd_pos pozice prikazu v argc
 * @return int ERR v pripade chybne zadaneho prikazu, jinak SUCC
 */
int cmd_process(int argc, char** argv, int cmd_pos, Params &params);

/**
 * @brief kontroluje zda zadany retezec neni prikaz
 * @param argv testovany retezec
 * @return int konstanta vyjadrujici prikaz, jinak CMD_INVALID
 */
int is_command(char *arg);

/**
 * @brief vraci pocet argumentu prikazu
 *
 * @param cmd prikaz
 * @return uint pocet argumentu
 */
uint cmd_args_num(int cmd);

/**
 * @brief spojeni se serverem
 *
 * @param params parametry, ip adresa, port a prikaz
 * @param args argumenty prikazu
 * @return int ERR v pripade nepodarene komunikace, jinak SUCC
 */
int connect_to_server(Params &params, char **args);

/**
 * @brief zajistuje vytvoreni zpravy podle protokolu a odelsani
 *
 * @param sockfd socket
 * @param cmd prikaz
 * @param args argumenty prikazu
 * @return int SUCC v pripade uspechu, jinak ERR
 */
int send_message(int sockfd, int cmd, char **args);

/**
 * @brief zajistuje prijeti zpravy a vypsani 
 *
 * @param sockfd socket
 * @param cmd prikaz
 * @return int SUCC v pripade uspechu, jinak ERR
 */
int recv_message(int sockfd, int cmd);

/**
 * @brief vytvari zpravu z parametru, tak aby vyhovovala protokolu
 *
 * @param cmd prikaz
 * @param args argumenty prikazu
 * @param request vysledna zprava odeslinana serveru
 * @return int SUCC v pripade uspechu, jinak ERR
 */
int build_request(int cmd, char **args, char *request);

/**
 * @brief vypis prijate odpovedi
 *
 * @param cmd prikaz
 * @param response odpoved serveru
 * @param response_len delka odpovedi
 * @return int SUCC v pripade uspechu, jinak ERR
 */
int parse_response(int cmd, char *response, int response_len);

/**
 * @brief konverze retezce na int
 *
 * @param str retezec
 * @param num vysledna hodnota
 * @return int ERR pokud nastala pri konverzi chyba, jinak SUCC
 */
int str2int(char* str, int &num);

/**
 * @brief fsm, ktery nalezne text mezi ""
 *
 * @param str retezec
 * @param index pocatecni index
 * @param end_index index konce retezce
 * @param state pocatecni stav fsm
 */
void text_fsm(char *str, int &index, int &end_index, int state);

int escape_backslash(char *text, int len);

int unescape_backslash(char *text, int len);

#endif
