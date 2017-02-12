#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "value.h"
#include "ds.h"
#include "vm.h"

/* Print the bytecode for a FuncDef */
static void FuncDefBytecodePrint(FuncDef * def) {
    uint32_t count, i;
    count = def->byteCodeLen;
    printf("(bytecode)[");
    if (count) {
        for (i = 0; i < count - 1; ++i) {
            printf("%04x ", def->byteCode[i]);
        }
        printf("%04x", def->byteCode[i]);
    }
    printf("]");
}

/* Print a value recursively. Used for debugging */
void ValuePrint(Value x, uint32_t indent) {
    uint32_t i;
    for (i = 0; i < indent; ++i)
        fputc(' ', stdout);
    switch (x.type) {
        case TYPE_NIL:
            printf("<nil>");
            break;
        case TYPE_BOOLEAN:
            printf(x.data.boolean ? "<true>" : "<false>");
            break;
        case TYPE_NUMBER:
            printf("%f", x.data.number);
            break;
        case TYPE_FORM:
        case TYPE_ARRAY:
            if (x.type == TYPE_ARRAY) printf("  [\n"); else printf("  (\n");
            for (i = 0; i < x.data.array->count; ++i) {
                ValuePrint(x.data.array->data[i], indent + 4);
                printf("\n");
            }
            for (i = 0; i < indent; ++i) fputc(' ', stdout);
            if (x.type == TYPE_ARRAY) printf("  ]\n"); else printf("  )\n");
            break;
        case TYPE_STRING:
            printf("\"%.*s\"", VStringSize(x.data.string), (char *) x.data.string);
            break;
        case TYPE_SYMBOL:
            printf("%.*s", VStringSize(x.data.string), (char *) x.data.string);
            break;
        case TYPE_CFUNCTION:
            printf("<cfunction>");
            break;
        case TYPE_FUNCTION:
            printf("<function ");
            FuncDefBytecodePrint(x.data.func->def);
            printf(">");
            break;
        case TYPE_DICTIONARY:
            printf("<dictionary>");
            break;
        case TYPE_BYTEBUFFER:
            printf("<bytebuffer>");
            break;
        case TYPE_FUNCDEF:
            printf("<funcdef ");
            FuncDefBytecodePrint(x.data.funcdef);
            printf(">");
            break;
        case TYPE_FUNCENV:
            printf("<funcenv>");
            break;
        case TYPE_THREAD:
            printf("<thread>");
            break;
    }
}

static uint8_t * LoadCString(VM * vm, const char * string, uint32_t len) {
    uint8_t * data = VMAlloc(vm, len + 2 * sizeof(uint32_t));
    data += 2 * sizeof(uint32_t);
    VStringHash(data) = 0;
    VStringSize(data) = len;
    memcpy(data, string, len);
    return data;
}

Value ValueLoadCString(VM * vm, const char * string) {
    Value ret;
    ret.type = TYPE_STRING;
    ret.data.string = LoadCString(vm, string, strlen(string));
    return ret;
}

static uint8_t * NumberToString(VM * vm, Number x) {
    static const uint32_t SIZE = 20;
    uint8_t * data = VMAlloc(vm, SIZE + 2 * sizeof(uint32_t));
    data += 2 * sizeof(uint32_t);
    snprintf((char *) data, SIZE, "%.17g", x);
    VStringHash(data) = 0;
    VStringSize(data) = strlen((char *) data);
    return data;
}

static const char * HEX_CHARACTERS = "0123456789ABCDEF";
#define HEX(i) (((uint8_t *) HEX_CHARACTERS)[(i)])

/* Returns a string description for a pointer */
static uint8_t * StringDescription(VM * vm, const char * title, uint32_t titlelen, void * pointer) {
    uint32_t len = 3 + titlelen + sizeof(pointer) * 2;
    uint32_t i;
    uint8_t * data = VMAlloc(vm, len + 2 * sizeof(uint32_t));
    uint8_t * c;
    union {
        uint8_t bytes[sizeof(void *)];
        void * p;
    } buf;
    buf.p = pointer;
    data += 2 * sizeof(uint32_t);
    c = data;
    *c++ = '<';
    for (i = 0; i < titlelen; ++i) {
        *c++ = ((uint8_t *)title) [i];
    }
    *c++ = ' ';
    for (i = 0; i < sizeof(void *); ++i) {
        uint8_t byte = buf.bytes[i];
        *c++ = HEX(byte >> 4);
        *c++ = HEX(byte & 0xF);
    }
    *c++ = '>';
    return data;
}

/* Returns a string pointer or NULL if could not allocate memory. */
uint8_t * ValueToString(VM * vm, Value x) {
    switch (x.type) {
        case TYPE_NIL:
            return LoadCString(vm, "nil", 3);
        case TYPE_BOOLEAN:
            if (x.data.boolean) {
                return LoadCString(vm, "true", 4);
            } else {
                return LoadCString(vm, "false", 5);
            }
        case TYPE_NUMBER:
            return NumberToString(vm, x.data.number);
        case TYPE_ARRAY:
            return StringDescription(vm, "array", 5, x.data.array);
        case TYPE_FORM:
            return StringDescription(vm, "form", 4, x.data.array);
        case TYPE_STRING:
        case TYPE_SYMBOL:
            return x.data.string;
        case TYPE_BYTEBUFFER:
            return StringDescription(vm, "buffer", 6, x.data.buffer);
        case TYPE_CFUNCTION:
            return StringDescription(vm, "cfunction", 9, x.data.cfunction);
        case TYPE_FUNCTION:
            return StringDescription(vm, "function", 8, x.data.func);
        case TYPE_DICTIONARY:
            return StringDescription(vm, "dictionary", 10, x.data.dict);
        case TYPE_FUNCDEF:
            return StringDescription(vm, "funcdef", 7, x.data.funcdef);
        case TYPE_FUNCENV:
            return StringDescription(vm, "funcenv", 7, x.data.funcenv);
        case TYPE_THREAD:
            return StringDescription(vm, "thread", 6, x.data.array);
    }
    return NULL;
}

/* Simple hash function */
uint32_t djb2(const uint8_t * str) {
    const uint8_t * end = str + VStringSize(str);
    uint32_t hash = 5381;
    while (str < end)
        hash = (hash << 5) + hash + *str++;
    return hash;
}

/* Check if two values are equal. This is strict equality with no conversion. */
int ValueEqual(Value x, Value y) {
    int result = 0;
    if (x.type != y.type) {
        result = 0;
    } else {
        switch (x.type) {
            case TYPE_NIL:
                result = 1;
                break;
            case TYPE_BOOLEAN:
                result = (x.data.boolean == y.data.boolean);
                break;
            case TYPE_NUMBER:
                result = (x.data.number == y.data.number);
                break;
                /* Assume that when strings are created, equal strings
                 * are set to the same string */
            case TYPE_STRING:
                /* TODO: keep cache to for symbols to get quick equality. */
            case TYPE_SYMBOL:
                if (x.data.string == y.data.string) {
                    result = 1;
                    break;
                }
                if (ValueHash(x) != ValueHash(y) ||
                        VStringSize(x.data.string) != VStringSize(y.data.string)) {
                    result = 0;
                    break;
                }
                if (!strncmp((char *) x.data.string, (char *) y.data.string, VStringSize(x.data.string))) {
                    result = 1;
                    break;
                }
                result = 0;
                break;
            case TYPE_ARRAY:
            case TYPE_FORM:
            case TYPE_BYTEBUFFER:
            case TYPE_CFUNCTION:
            case TYPE_DICTIONARY:
            case TYPE_FUNCTION:
            case TYPE_FUNCDEF:
            case TYPE_FUNCENV:
            case TYPE_THREAD:
                /* compare pointers */
                result = (x.data.array == y.data.array);
                break;
        }
    }
    return result;
}

/* Computes a hash value for a function */
uint32_t ValueHash(Value x) {
    uint32_t hash = 0;
    switch (x.type) {
        case TYPE_NIL:
            hash = 0;
            break;
        case TYPE_BOOLEAN:
            hash = x.data.boolean;
            break;
        case TYPE_NUMBER:
            {
                union {
                    uint32_t hash;
                    Number number;
                } u;
                u.number = x.data.number;
                hash = u.hash;
            }
            break;
            /* String hashes */
        case TYPE_SYMBOL:
        case TYPE_STRING:
            /* Assume 0 is not hashed. */
            if (VStringHash(x.data.string))
                hash = VStringHash(x.data.string);
            else
                hash = VStringHash(x.data.string) = djb2(x.data.string);
            break;
        case TYPE_ARRAY:
        case TYPE_FORM:
        case TYPE_BYTEBUFFER:
        case TYPE_CFUNCTION:
        case TYPE_DICTIONARY:
        case TYPE_FUNCTION:
        case TYPE_FUNCDEF:
        case TYPE_FUNCENV:
        case TYPE_THREAD:
            /* Cast the pointer */
            {
                union {
                    void * pointer;
                    uint32_t hash;
                } u;
                u.pointer = x.data.pointer;
                hash = u.hash;
            }
            break;
    }
    return hash;
}

/* Compares x to y. If they are equal retuns 0. If x is less, returns -1.
 * If y is less, returns 1. All types are comparable
 * and should have strict ordering. */
int ValueCompare(Value x, Value y) {
    if (x.type == y.type) {
        switch (x.type) {
            case TYPE_NIL:
                return 0;
            case TYPE_BOOLEAN:
                if (x.data.boolean == y.data.boolean) {
                    return 0;
                } else {
                    return x.data.boolean ? 1 : -1;
                }
            case TYPE_NUMBER:
                /* TODO: define behavior for NaN and infinties. */
                if (x.data.number == y.data.number) {
                    return 0;
                } else {
                    return x.data.number > y.data.number ? 1 : -1;
                }
            case TYPE_STRING:
            case TYPE_SYMBOL:
                if (x.data.string == y.data.string) {
                    return 0;
                } else {
                    uint32_t xlen = VStringSize(x.data.string);
                    uint32_t ylen = VStringSize(y.data.string);
                    uint32_t len = xlen > ylen ? ylen : xlen;
                    uint32_t i;
                    for (i = 0; i < len; ++i) {
                        if (x.data.string[i] == y.data.string[i]) {
                            continue;
                        } else if (x.data.string[i] < y.data.string[i]) {
                            return 1; /* x is less then y */
                        } else {
                            return -1; /* y is less than x */
                        }
                    }
                    if (xlen == ylen) {
                        return 0;
                    } else {
                        return xlen < ylen ? -1 : 1;
                    }
                }
            case TYPE_ARRAY:
            case TYPE_FORM:
            case TYPE_BYTEBUFFER:
            case TYPE_CFUNCTION:
            case TYPE_FUNCTION:
            case TYPE_DICTIONARY:
            case TYPE_FUNCDEF:
            case TYPE_FUNCENV:
            case TYPE_THREAD:
                if (x.data.string == y.data.string) {
                    return 0;
                } else {
                    return x.data.string > y.data.string ? 1 : -1;
                }
        }
    } else if (x.type < y.type) {
        return -1;
    }
    return 1;
}
