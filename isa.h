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
#define PORT_MIN 0  //!< minimalni hodnota portu

/**
 * @brief parametry z prikazove radky
 */
struct Params{
    char *addr;
    char *port;
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
int cmd_process(int argc, char** argv, int cmd_pos);

/**
 * @brief kontroluje zda zadani retezec neni prikaz
 * @param argv testovany retezec
 * @return bool TRUE pokud je to prikaz, jinak FALSE
 */
bool is_command(char *arg);

/**
 * @brief vraci pocet argumentu prikazu
 *
 * @param cmd prikaz
 * @return uint pocet argumentu
 */
uint cmd_args_num(char *cmd);

/**
 * @brief spojeni se serverem
 *
 * @param params parametry, ip adresa a port
 * @param command prikaz, ktery se ma vykonat
 * @param args argumenty prikazu
 * @return int ERR v pripade nepodarene komunikace, jinak SUCC
 */
int connect_to_server(Params &params, char *command, char *args, int argc);

int str2int(char* str, int &num);

#endif