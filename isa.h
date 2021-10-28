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
 * @return ERR v pripade chyby, SUCC - uspech, jinak pozice prikazu (command)
 */
int arg_process(int argc, char** argv, Params &params);

/**
 * @brief kontroluje zda zadani retezec neni prikaz
 * @param argv testovany retezec
 * @return TRUE pokud je to prikaz, jinak FALSE
 */
bool is_command(char *arg);

#endif