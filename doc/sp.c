/*
 * Copyright (c) 2009-2016 Petri Lehtinen <petri@digip.org>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <jansson.h>

#define BUFFER_SIZE (256 * 1024) /* 256 KB */

#define URL_FORMAT "https://api.github.com/repos/%s/%s/commits"
#define URL_SIZE   256

/* Return the offset of the first newline in text or the length of
   text if there's no newline */
static int newline_offset(const char *text) {
    const char *newline = strchr(text, '\n');
    if (!newline)
        return strlen(text);
    else
        return (int)(newline - text);
}

struct write_result {
    char *data;
    int pos;
};

static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream) {
    struct write_result *result = (struct write_result *)stream;

    if (result->pos + size * nmemb >= BUFFER_SIZE - 1) {
        fprintf(stderr, "error: too small buffer\n");
        return 0;
    }

    memcpy(result->data + result->pos, ptr, size * nmemb);
    result->pos += size * nmemb;

    return size * nmemb;
}

static char *request(const char *url) {
    CURL *curl = NULL;
    CURLcode status;
    struct curl_slist *headers = NULL;
    char *data = NULL;
    long code;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (!curl)
        goto error;

    data = malloc(BUFFER_SIZE);
    if (!data)
        goto error;

    struct write_result write_result = {.data = data, .pos = 0};

    curl_easy_setopt(curl, CURLOPT_URL, url);

    /* GitHub commits API v3 requires a User-Agent header */
    headers = curl_slist_append(headers, "User-Agent: Jansson-Tutorial");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);

    status = curl_easy_perform(curl);
    if (status != 0) {
        fprintf(stderr, "error: unable to request data from %s:\n", url);
        fprintf(stderr, "%s\n", curl_easy_strerror(status));
        goto error;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if (code != 200) {
        fprintf(stderr, "error: server responded with code %ld\n", code);
        goto error;
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    curl_global_cleanup();

    /* zero-terminate the result */
    data[write_result.pos] = '\0';

    return data;

error:
    if (data)
        free(data);
    if (curl)
        curl_easy_cleanup(curl);
    if (headers)
        curl_slist_free_all(headers);
    curl_global_cleanup();
    return NULL;
}

const char *json_plural(size_t count) { return count == 1 ? "" : "s"; }

json_t *load_json(const char *text) {
    json_t *root;
    json_error_t error;

    root = json_loads(text, 0, &error);

    if (root) {
        return root;
    } else {
        fprintf(stderr, "json error on line %d: %s\n", error.line, error.text);
        return (json_t *)0;
    }
}
void print_json(json_t *root);
void print_json_aux(json_t *element, int indent);
void print_json_false(json_t *element, int indent);
void print_json_indent(int indent);
void print_json_integer(json_t *element, int indent);
void print_json_null(json_t *element, int indent);
void print_json_object(json_t *element, int indent);
void print_json_real(json_t *element, int indent);
void print_json_string(json_t *element, int indent);
void print_json_true(json_t *element, int indent);


void print_json(json_t *root) { print_json_aux(root, 0); }

void print_json_array(json_t *element, int indent) {
    size_t i;
    size_t size = json_array_size(element);
    print_json_indent(indent);

    printf("JSON Array of %lld element%s:\n", (long long)size, json_plural(size));
    for (i = 0; i < size; i++) {
        print_json_aux(json_array_get(element, i), indent + 2);
    }
}

void print_json_aux(json_t *element, int indent) {
    switch (json_typeof(element)) {
        case JSON_OBJECT:
            print_json_object(element, indent);
            break;
        case JSON_ARRAY:
            print_json_array(element, indent);
            break;
        case JSON_STRING:
            print_json_string(element, indent);
            break;
        case JSON_INTEGER:
            print_json_integer(element, indent);
            break;
        case JSON_REAL:
            print_json_real(element, indent);
            break;
        case JSON_TRUE:
            print_json_true(element, indent);
            break;
        case JSON_FALSE:
            print_json_false(element, indent);
            break;
        case JSON_NULL:
            print_json_null(element, indent);
            break;
        default:
            fprintf(stderr, "unrecognized JSON type %d\n", json_typeof(element));
    }
}
void print_json_false(json_t *element, int indent) {
    (void)element;
    print_json_indent(indent);
    printf("JSON False\n");
}

void print_json_indent(int indent) {
    int i;
    for (i = 0; i < indent; i++) {
        putchar(' ');
    }
}
void print_json_integer(json_t *element, int indent) {
    print_json_indent(indent);
    printf("JSON Integer: \"%" JSON_INTEGER_FORMAT "\"\n", json_integer_value(element));
}
void print_json_null(json_t *element, int indent) {
    (void)element;
    print_json_indent(indent);
    printf("JSON Null\n");
}
void print_json_object(json_t *element, int indent) {
    size_t size;
    const char *key;
    json_t *value;

    print_json_indent(indent);
    size = json_object_size(element);

    printf("JSON Object of %lld pair%s:\n", (long long)size, json_plural(size));
    json_object_foreach(element, key, value) {
        print_json_indent(indent + 2);
        printf("JSON Key: \"%s\"\n", key);
        print_json_aux(value, indent + 2);
    }
}

void print_json_real(json_t *element, int indent) {
    print_json_indent(indent);
    printf("JSON Real: %f\n", json_real_value(element));
}

void print_json_string(json_t *element, int indent) {
    print_json_indent(indent);
    printf("JSON String: \"%s\"\n", json_string_value(element));
}

void print_json_true(json_t *element, int indent) {
    (void)element;
    print_json_indent(indent);
    printf("JSON True\n");
}

char *read_line(char *line, int max_chars) {
    printf("Type some JSON > ");
    fflush(stdout);
    return fgets(line, max_chars, stdin);
}

#define MAX_CHARS 4096

int main(int argc, char *argv[]) {
    size_t i;
    char *text;
    char url[URL_SIZE];
    char line[MAX_CHARS];


    json_t *root;
    json_error_t error;

    if (argc != 3) {
        fprintf(stderr, "usage: %s USER REPOSITORY\n\n", argv[0]);
        fprintf(stderr, "List commits at USER's REPOSITORY.\n\n");
        return 2;
    }

    snprintf(url, URL_SIZE, URL_FORMAT, argv[1], argv[2]);

    text = request(url);
    if (!text)
        return 1;

    root = json_loads(text, 0, &error);
    free(text);

    if (!root) {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        return 1;
    }

    if (!json_is_array(root)) {
        fprintf(stderr, "error: root is not an array\n");
        json_decref(root);
        return 1;
    }

    for (i = 0; i < json_array_size(root); i++) {
        json_t *data, *sha, *commit, *message;
        const char *message_text;

        data = json_array_get(root, i);
        if (!json_is_object(data)) {
            fprintf(stderr, "error: commit data %d is not an object\n", (int)(i + 1));
            json_decref(root);
            return 1;
        }

        sha = json_object_get(data, "sha");
        if (!json_is_string(sha)) {
            fprintf(stderr, "error: commit %d: sha is not a string\n", (int)(i + 1));
            json_decref(root);
            return 1;
        }

        commit = json_object_get(data, "commit");
        if (!json_is_object(commit)) {
            fprintf(stderr, "error: commit %d: commit is not an object\n", (int)(i + 1));
            json_decref(root);
            return 1;
        }

        message = json_object_get(commit, "message");
        if (!json_is_string(message)) {
            fprintf(stderr, "error: commit %d: message is not a string\n", (int)(i + 1));
            json_decref(root);
            return 1;
        }

        message_text = json_string_value(message);
        printf("%.8s %.*s\n", json_string_value(sha), newline_offset(message_text),
               message_text);
    }

    json_decref(root);

    while (read_line(line, MAX_CHARS) != (char *)NULL) {

        /* parse text into JSON structure */
        json_t *root = load_json(line);

        if (root) {
            /* print and release the JSON structure */
            print_json(root);
            json_decref(root);
        }
    }

    return 0;
}
