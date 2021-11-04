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

#define PORT_MAX 65535  //!< maximalni hodnota portu
#define PORT_MIN 1   //!< minimalni hodnota portu

#define CMD_INVALID -1
#define CMD_REGISTER 10
#define CMD_LOGIN 11
#define CMD_LOGOUT 12
#define CMD_SEND 13
#define CMD_FETCH 14
#define CMD_LIST 15

#define CMD_ARGC_3 3
#define CMD_ARGC_2 2
#define CMD_ARGC_1 1
#define CMD_ARGC_0 0

#define FETCH_ARGS 3
#define LIST_ARGS 3

#define MSG_MAX_LEN 16384

#define SKIP_BYTE 1
#define MSG_ERR_STATUS_LEN 5
#define MSG_SUCC_STATUS_LEN 4

#define MSG_TEXT 20
#define MSG_ESCAPE_SEQ 21
#define MSG_NOT_TEXT 22
#define MSG_START 23
#define MSG_END 24

/**
 * @brief parametry z prikazove radky
 */
struct Params{
    char *addr;
    char *port;
    int cmd;
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
 * @brief kontroluje zda zadani retezec neni prikaz
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

int send_message(int sockfd, int cmd, char **args);

int recv_message(int sockfd, int cmd);

int build_request(int cmd, char **args, char *request);

int parse_response(int cmd, char *response, int response_len);

int str2int(char* str, int &num);

int text_fsm(char *str, int &index, int &end_index, int state);

#endif