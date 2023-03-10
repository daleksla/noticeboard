#ifndef CONSTRAINTS_H
#define CONSTRAINTS_H
#pragma once

/**
 * @brief Header-only macros for constraining message lengths
 * This isn't to be included directly - request and response will utilise these, respectively
 */

#define MAX_SBJ_LEN 30 /* maximum subject length. excludes NULL terminator */
#define MAX_EXTRA_DATA_LEN 2000 /* limit messages to 2000 characters. excludes NULL terminator */

#endif /* CONSTRAINTS_H */
