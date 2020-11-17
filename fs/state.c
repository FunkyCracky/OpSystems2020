#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "state.h"
#include "../tecnicofs-api-constants.h"

/*
 * Sleeps for synchronization testing.
 */
void insert_delay(int cycles) {
    for (int i = 0; i < cycles; i++) {}
}

/*
 * Initializes the i-nodes table.
 */
void inode_table_init() {
    for (int i = 0; i < INODE_TABLE_SIZE; i++) {
        inode_table[i].nodeType = T_NONE;
        inode_table[i].data.dirEntries = NULL;
        inode_table[i].data.fileContents = NULL;
        if (pthread_rwlock_init(&inode_table[i].rwl, NULL)) {
            fprintf(stderr, "Error initializing inode %d rwlock!\n", i);
            exit(EXIT_FAILURE);
        }
    }
}

/*
 * Releases the allocated memory for the i-nodes tables.
 */
void inode_table_destroy() {
    for (int i = 0; i < INODE_TABLE_SIZE; i++) {
        if (inode_table[i].nodeType != T_NONE) {
            /* as data is an union, the same pointer is used for both dirEntries and fileContents */
            /* just release one of them */
	        if (inode_table[i].data.dirEntries) {
                free(inode_table[i].data.dirEntries);
            }
        }
        if (pthread_rwlock_destroy(&inode_table[i].rwl)) {
            printf("Error destroying node %d rwlock!\n", i);
        }
    } 
}

/*
 * Creates a new i-node in the table with the given information.
 * Input:
 *  - nType: the type of the node (file or directory)
 * Returns:
 *  inumber: identifier of the new i-node, if successfully created
 *     FAIL: if an error occurs
 */

int inode_create(type nType) {
    /* Used for testing synchronization speedup */
    insert_delay(DELAY);

    for (int inumber = 0; inumber < INODE_TABLE_SIZE; inumber++) {
        int lockReturn = pthread_rwlock_trywrlock(&(inode_table[inumber].rwl));
        if (lockReturn == 0) {
            if (inode_table[inumber].nodeType == T_NONE) {
                inode_table[inumber].nodeType = nType;
                if (nType == T_DIRECTORY) {
                    /* Initializes entry table */
                    inode_table[inumber].data.dirEntries = malloc(sizeof(DirEntry) * MAX_DIR_ENTRIES);
                    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
                        inode_table[inumber].data.dirEntries[i].inumber = FREE_INODE;
                    }
                } else {
                    inode_table[inumber].data.fileContents = NULL;
                }
                return inumber;
            }
            unlock(inumber);
        } else if (lockReturn != EBUSY) {
            fprintf(stderr, "Error locking inode %d\n", inumber);
            exit(EXIT_FAILURE);
        }
    }
    return FAIL;
}

/*
 * Deletes the i-node.
 * Input:
 *  - inumber: identifier of the i-node
 * Returns: SUCCESS or FAIL
 */
int inode_delete(int inumber) {
    /* Used for testing synchronization speedup */
    insert_delay(DELAY);

    if ((inumber < 0) || (inumber > INODE_TABLE_SIZE) || (inode_table[inumber].nodeType == T_NONE)) {
        printf("inode_delete: invalid inumber\n");
        return FAIL;
    }
    
    inode_table[inumber].nodeType = T_NONE;
    /* see inode_table_destroy function */
    /* ^^ -> it frees the dirEntries and fileContents (union) */
    if (inode_table[inumber].data.dirEntries) {
        free(inode_table[inumber].data.dirEntries);
    }

    return SUCCESS;
}

/*
 * Copies the contents of the i-node into the arguments.
 * Only the fields referenced by non-null arguments are copied.
 * Input:
 *  - inumber: identifier of the i-node
 *  - nType: pointer to type
 *  - data: pointer to data
 * Returns: SUCCESS or FAIL
 */
int inode_get(int inumber, type *nType, union Data *data) {
    /* Used for testing synchronization speedup */
    insert_delay(DELAY);

    if ((inumber < 0) || (inumber > INODE_TABLE_SIZE) || (inode_table[inumber].nodeType == T_NONE)) {
        printf("inode_get: invalid inumber %d\n", inumber);
 
        return FAIL;
    }

    if (nType)
        *nType = inode_table[inumber].nodeType;

    if (data)
        *data = inode_table[inumber].data;

    return SUCCESS;
}


/*
 * Looks for node in directory entry from name.
 * Input:
 *  - name: path of node
 *  - entries: entries of directory
 * Returns:
 *  - inumber: found node's inumber
 *  - FAIL: if not found
 */
int lookup_sub_node(char *name, DirEntry *entries) {
 
	if (entries == NULL) {
		return FAIL;
	}
	for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (entries[i].inumber != FREE_INODE && strcmp(entries[i].name, name) == 0) {
            return entries[i].inumber;
        }
    }
	
	return FAIL;
}


/*
 * Resets an entry for a directory.
 * Input:
 *  - inumber: identifier of the i-node
 *  - sub_inumber: identifier of the sub i-node entry
 * Returns: SUCCESS or FAIL
 */
int dir_reset_entry(int inumber, int sub_inumber) {
    /* Used for testing synchronization speedup */
    insert_delay(DELAY);


    if ((inumber < 0) || (inumber > INODE_TABLE_SIZE) || (inode_table[inumber].nodeType == T_NONE)) {
        printf("inode_reset_entry: invalid inumber\n");
        return FAIL;
    }

    if (inode_table[inumber].nodeType != T_DIRECTORY) {
        printf("inode_reset_entry: can only reset entry to directories\n");
        return FAIL;
    }

    if ((sub_inumber < FREE_INODE) || (sub_inumber > INODE_TABLE_SIZE) || (inode_table[sub_inumber].nodeType == T_NONE)) {
        printf("inode_reset_entry: invalid entry inumber\n");
        return FAIL;
    }

    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (inode_table[inumber].data.dirEntries[i].inumber == sub_inumber) {
            inode_table[inumber].data.dirEntries[i].inumber = FREE_INODE;
            inode_table[inumber].data.dirEntries[i].name[0] = '\0';
            return SUCCESS;
        }
    }

    return FAIL;
}


/*
 * Adds an entry to the i-node directory data.
 * Input:
 *  - inumber: identifier of the i-node
 *  - sub_inumber: identifier of the sub i-node entry
 *  - sub_name: name of the sub i-node entry 
 * Returns: SUCCESS or FAIL
 */
int dir_add_entry(int inumber, int sub_inumber, char *sub_name) {
    /* Used for testing synchronization speedup */
    insert_delay(DELAY);
    
    /* invalid parent inumber */
    if ((inumber < 0) || (inumber > INODE_TABLE_SIZE) || (inode_table[inumber].nodeType == T_NONE)) {
        printf("inode_add_entry: invalid inumber\n");
        return FAIL;
    }

    /* parent is not a directory */
    if (inode_table[inumber].nodeType != T_DIRECTORY) {
        printf("inode_add_entry: can only add entry to directories\n");
        return FAIL;
    }

    /* invalid child inumber */
    /* at this point, nodeType is already set at nType */
    if ((sub_inumber < 0) || (sub_inumber > INODE_TABLE_SIZE) || (inode_table[sub_inumber].nodeType == T_NONE)) {
        printf("inode_add_entry: invalid entry inumber\n");
        return FAIL;
    }

    /* invalid sub_name */
    if (strlen(sub_name) == 0 ) {
        printf("inode_add_entry: \
               entry name must be non-empty\n");
        return FAIL;
    }

    /* iterate through table */
    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        /* find first entry which is free */
        if (inode_table[inumber].data.dirEntries[i].inumber == FREE_INODE) {
            /* update its data */
            /* dirEntry is composed of name and i-number */
            inode_table[inumber].data.dirEntries[i].inumber = sub_inumber;
            strcpy(inode_table[inumber].data.dirEntries[i].name, sub_name);
            return SUCCESS;
        }
    }
    
    return FAIL;
}

/**
 * Locks i-node rwlock.
 * Input:
 *  - inumber: number of the i-node being locked
 *  - lockType: READ or WRITE 
 * Returns: SUCCESS or FAIL
 */
void lock(int inumber, int lockType) {
    if (lockType == READ) {
        if (pthread_rwlock_rdlock(&(inode_table[inumber].rwl))) {
            fprintf(stderr, "failed to lockRead(%d)\n", inumber);
            exit(EXIT_FAILURE);
        }
    }
    else if (lockType == WRITE) {
        if (pthread_rwlock_wrlock(&(inode_table[inumber].rwl))) {
            fprintf(stderr, "failed to lockWrite(%d)\n", inumber);
            exit(EXIT_FAILURE);
        }
    }
}

/**
 * Unlocks i-node rwlock.
 * Input:
 *  - inumber: number of the i-node being unlocked.
 */
void unlock(int inumber) {
    if (pthread_rwlock_unlock(&inode_table[inumber].rwl)) { 
        fprintf(stderr, "Error unlocking inode %d's rwl!\n", inumber);
        exit(EXIT_FAILURE);
    }
}

/**
 * Unlocks array of inumbers.
 * Input:
 *  - inumbers: array containing locked i-nodes
 *  - size: length of inumbers' array
 */
void unlockAll(int inumbers[], int size) {
    for (int i = size-1; i >= 0; --i) {
        unlock(inumbers[i]);
    }
}



/*
 * Prints the i-nodes table.
 * Input:
 *  - inumber: identifier of the i-node
 *  - name: pointer to the name of current file/dir
 */
void inode_print_tree(FILE *fp, int inumber, char *name) {
    if (inode_table[inumber].nodeType == T_FILE) {
        fprintf(fp, "%s\n", name);
        return;
    }

    if (inode_table[inumber].nodeType == T_DIRECTORY) {
        fprintf(fp, "%s\n", name);
        for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
            if (inode_table[inumber].data.dirEntries[i].inumber != FREE_INODE) {
                char path[MAX_FILE_NAME];
                if (snprintf(path, sizeof(path), "%s/%s", name, inode_table[inumber].data.dirEntries[i].name) > sizeof(path)) {
                    fprintf(stderr, "truncation when building full path\n");
                }
                inode_print_tree(fp, inode_table[inumber].data.dirEntries[i].inumber, path);
            }
        }
    }
}
