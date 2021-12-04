#include "caldav.h"

CURL *curl;
char* access_token;
char* refresh_token;
int token_ttl;

// Local bind address for receiving OAuth callbacks.
// Reserve 2 chars for the ipv6 square brackets.
char ip[INET6_ADDRSTRLEN], ipstr[INET6_ADDRSTRLEN+2];

/* Write a random code challenge of size len to dest */
void random_code_challenge(size_t len, char* dest) {
    // https://developers.google.com/identity/protocols/oauth2/native-app#create-code-challenge
    // A code_verifier is a random string using characters [A-Z] / [a-z] / [0-9] / "-" / "." / "_" / "~"
    srand(time(NULL));

    char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    size_t alphabet_size = strlen(alphabet);

    int i = 0;
    for (i = 0; i < len; i++) {
        dest[i] = alphabet[rand() % alphabet_size];
    }
    dest[len-1] = '\0';
}

static size_t curl_write_mem_callback(void * contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct curl_mem_chunk* mem = (struct curl_mem_chunk*)userp;

    char* ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "not enough memory (realloc in CURLOPT_WRITEFUNCTION returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
}

// todo
// https://beej.us/guide/bgnet/html
void* get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/*
* Extract OAuth2 code from http header. Reads the code from
* the http header in http_header and writes the OAuth2 code
* to the char string pointed to by code.
*/
char* extract_oauth_code(char* http_header) {
    // example token: "/?code=&scope="
    char* res = strtok(http_header, " ");
    while (res != NULL) {
        if (strstr(res, "code") != NULL) {
            res = strtok(res, "="); // code key
            res = strtok(NULL, "&"); // code value
            fprintf(stderr, "Code: %s\n", res);
            break;
        }
        res = strtok(NULL, " ");
    }
    return res;
}

char* read_tokenfile() {
    FILE* token_file;
    char* token_buf;
    long token_bytes;

    char* tokenfile_path = expand_path(CONFIG.google_tokenfile);
    token_file = fopen(tokenfile_path, "r");
    free(tokenfile_path);

    if (token_file == NULL) {
        perror("Failed to open tokenfile");
        return NULL;
    }

    fseek(token_file, 0, SEEK_END);
    token_bytes = ftell(token_file);
    rewind(token_file);

    token_buf = malloc(token_bytes + 1);
    if (token_buf != NULL) {
        fread(token_buf, sizeof(char), token_bytes, token_file);
        token_buf[token_bytes] = '\0';

        access_token = extract_json_value(token_buf, "access_token", true);

        // program segfaults if NULL value is provided to atoi
        char* token_ttl_str = extract_json_value(token_buf, "expires_in", false);
        if (token_ttl_str == NULL) {
            token_ttl = 0;
        } else {
            token_ttl = atoi(token_ttl_str);
        }

        // only update the existing refresh token if the request actually
        // contained a valid refresh_token, i.e, if it was the initial
        // interactive authZ request from token code confirmed by the user
        char* new_refresh_token = extract_json_value(token_buf, "refresh_token", true);
        if (new_refresh_token != NULL) {
            refresh_token = new_refresh_token;
        }

        fprintf(stderr, "Access token: %s\n", access_token);
        fprintf(stderr, "Token TTL: %i\n", token_ttl);
        fprintf(stderr, "Refresh token: %s\n", refresh_token);
    } else {
        perror("malloc failed");
        return NULL;
    }
    fclose(token_file);
    return token_buf;
}

void write_tokenfile() {
    char* tokenfile_path = expand_path(CONFIG.google_tokenfile);
    FILE* tokenfile = fopen(tokenfile_path, "wb");
    free(tokenfile_path);

    if (tokenfile == NULL) {
        perror("Failed to open tokenfile");
    } else {
        char contents[1000];
        char* tokenfile_contents = "{\n"
        "  \"access_token\": \"%s\",\n"
        "  \"expires_in\": %i,\n"
        "  \"refresh_token\": \"%s\"\n"
        "}\n";
        sprintf(contents, tokenfile_contents,
                access_token,
                token_ttl,
                refresh_token);
        fputs(contents, tokenfile);
    }
    fclose(tokenfile);

    chmod(tokenfile_path, S_IRUSR|S_IWUSR);
    perror("chmod");


    char* tokfile = read_tokenfile();
    fprintf(stderr, "New tokenfile contents: %s\n", tokfile);
    fprintf(stderr, "New Access token: %s\n", access_token);
    fprintf(stderr, "New Token TTL: %i\n", token_ttl);
    fprintf(stderr, "Refresh token: %s\n", refresh_token);
    free(tokfile);
}

void get_access_token(char* code, char* verifier, bool refresh) {
    CURLcode res;
    char* tokfile;

    char postfields[500];
    if (refresh) {
        sprintf(postfields, "client_id=%s&client_secret=%s&grant_type=refresh_token&refresh_token=%s",
                CONFIG.google_clientid,
                CONFIG.google_secretid,
                refresh_token);
    } else {
        sprintf(postfields, "client_id=%s&client_secret=%s&code=%s&code_verifier=%s&grant_type=authorization_code&redirect_uri=http://%s:%i",
                CONFIG.google_clientid,
                CONFIG.google_secretid,
                code,
                verifier,
                ipstr,
                GOOGLE_OAUTH_REDIRECT_PORT);
    }
    fprintf(stderr, "CURLOPT_POSTFIELDS: %s\n", postfields);

    curl = curl_easy_init();

    FILE* tokenfile;
    char* tokenfile_path;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, GOOGLE_OAUTH_TOKEN_URL);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

        tokenfile_path = expand_path(CONFIG.google_tokenfile);
        tokenfile = fopen(tokenfile_path, "wb");
        free(tokenfile_path);

        if (tokenfile == NULL) {
            perror("Failed to open tokenfile");
        } else {
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, tokenfile);
            res = curl_easy_perform(curl);
            fclose(tokenfile);
        }

        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            return;
        }
        // update global variables from tokenfile
        // this will also init the access_token var the first time
        tokfile = read_tokenfile();
        free(tokfile);
        // Make sure the refresh token is re-written and persistet
        // to the tokenfile for further requests, because the
        // is not returned by the refresh_token call:
        // https://developers.google.com/identity/protocols/oauth2/native-app#offline
        write_tokenfile();
    }
}

char* get_oauth_code(const char* verifier, WINDOW* header) {
    struct addrinfo hints, *addr_res;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status;
    if ((status=getaddrinfo(NULL, MKSTR(GOOGLE_OAUTH_REDIRECT_PORT), &hints, &addr_res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    }

    void *addr;
    char *ipver;
    //todo: extract
    //addr = get_in_addr(addr_res->ai_addr);
    if (addr_res->ai_family == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *) addr_res->ai_addr;
        addr = &(ipv4->sin_addr);
        ipver = "IPv4";
    } else { // IPv6
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *) addr_res->ai_addr;
        addr = &(ipv6->sin6_addr);
        ipver = "IPv6";
    }

    inet_ntop(addr_res->ai_family, addr, ip, sizeof ip);
    if (strcmp("IPv6", ipver) == 0) {
        sprintf(ipstr, "[%s]", ip);
    }

    // Show Google OAuth URI
    char uri[500];
    sprintf(uri, "%s?scope=%s&code_challenge=%s&response_type=%s&redirect_uri=http://%s:%i&client_id=%s",
            GOOGLE_OAUTH_AUTHZ_URL,
            GOOGLE_OAUTH_SCOPE,
            verifier,
            GOOGLE_OAUTH_RESPONSE_TYPE,
            ipstr,
            GOOGLE_OAUTH_REDIRECT_PORT,
            CONFIG.google_clientid);
    fprintf(stderr, "Google OAuth2 authorization URI: %s\n", uri);

    // Show the Google OAuth2 authorization URI in the header
    wclear(header);
    wresize(header, LINES, getmaxx(header));
    mvwprintw(header, 0, 0, "Go to Google OAuth2 authorization URI. Use 'q' or 'Ctrl+c' to quit authorization process.\n%s", uri);
    wrefresh(header);

    int socketfd = socket(addr_res->ai_family, addr_res->ai_socktype, addr_res->ai_protocol);
    if (socketfd < 0) {
       perror("Error opening socket");
    }

    // reuse socket address
    int yes=1;
    setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    if (bind(socketfd, addr_res->ai_addr, addr_res->ai_addrlen) < 0) {
       perror("Error binding socket");
    }

    freeaddrinfo(addr_res);

    int ls = listen(socketfd, GOOGLE_OAUTH_REDIRECT_SOCKET_BACKLOG);
    if (ls < 0) {
       perror("Listen error");
    }

    struct pollfd pfds[2];

    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN;
    pfds[1].fd = socketfd;
    pfds[1].events = POLLIN;
    int fd_count = 2;
            
    int connfd, bytes_rec, bytes_sent;
    char http_header[8*1024];
    char* reply =
    "HTTP/1.1 200 OK\n"
    "Content-Type: text/html\n"
    "Connection: close\n\n"
    "<html>"
    "<head><title>Authorization successfull</title></head>"
    "<body>"
    "<p><b>Authorization successfull.</b></p>"
    "<p>You consented that diary can access your Google calendar.<br/>"
    "Pleasee close this window and return to diary.</p>"
    "</body>"
    "</html>";

    // Handle descriptors read-to-read (POLLIN),
    // stdin or server socker, whichever is first
    for (;;) {
        int poll_count = poll(pfds, fd_count, -1);

        if (poll_count == -1) {
            perror("Erro in poll");
            break;
        }

        // Cancel through stdin
        if (pfds[0].revents & POLLIN) {
            noecho();
            int ch = wgetch(header);
            echo();
            // sudo showkey -a 
            // Ctrl+c: ^C 0x03
            // q     :  q 0x71
            if (ch == 0x03 || ch == 0x71) {
                fprintf(stderr, "Escape char: %x\n", ch);
                fprintf(stderr, "Hanging up, closing server socket\n");
                break;
            }
        }
        if (pfds[1].revents & POLLIN) {
            // accept connections but ignore client addr
            connfd = accept(socketfd, NULL, NULL);
            if (connfd < 0) {
               perror("Error accepting connection");
               break;
            }

            bytes_rec = recv(connfd, http_header, sizeof http_header, 0);
            if (bytes_rec < 0) {
                perror("Error reading stream message");
                break;
            }
            fprintf(stderr, "Received http header: %s\n", http_header);

            bytes_sent = send(connfd, reply, strlen(reply), 0);
            if (bytes_sent < 0) {
               perror("Error sending");
            }
            fprintf(stderr, "Bytes sent: %i\n", bytes_sent);

            close(connfd);
            break;
        }
    } // end for ;;

    // close server socket
    close(pfds[1].fd);

    char* code = extract_oauth_code(http_header);
    if (code == NULL) {
        fprintf(stderr, "Found no OAuth code in http header.\n");
        return NULL;
    }
    fprintf(stderr, "OAuth code: %s\n", code);

    return code;
}

char* caldav_req(struct tm* date, char* url, char* http_method, char* postfields, int depth) {
    // only support depths 0 or 1
    if (depth < 0 || depth > 1) {
        return NULL;
    }

    // if access_token is NULL, the program segfaults
    // while construcing the bearer_token below
    if (access_token == NULL) {
        return NULL;
    }

    CURLcode res;

    curl = curl_easy_init();

    // https://curl.se/libcurl/c/getinmemory.html
    struct curl_mem_chunk caldav_resp;
    caldav_resp.memory = malloc(1);
    if (caldav_resp.memory == NULL) {
        perror("malloc failed");
        return NULL;
    }
    caldav_resp.size = 0;

    if (curl) {
        // fail if not authenticated, !CURLE_OK
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, http_method);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_mem_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&caldav_resp);

        // construct header fields
        struct curl_slist *header = NULL;
        char bearer_token[strlen("Authorization: Bearer")+strlen(access_token)];
        sprintf(bearer_token, "Authorization: Bearer %s", access_token);
        char depth_header[strlen("Depth: 0")];
        sprintf(depth_header, "Depth: %i", depth);
        header = curl_slist_append(header, depth_header);
        header = curl_slist_append(header, bearer_token);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);

        // set postfields, if any
        if (postfields != NULL) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
        }

        res = curl_easy_perform(curl);

        curl_easy_cleanup(curl);

        fprintf(stderr, "Curl retrieved %lu bytes\n", (unsigned long)caldav_resp.size);
        fprintf(stderr, "Curl content: %s\n", caldav_resp.memory);

        if (res != CURLE_OK) {
            fprintf(stderr, "Curl response: %s\n", caldav_resp.memory);
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            return NULL;
        }
    }

    return caldav_resp.memory;
}

// return current user principal from CalDAV XML response
char* parse_caldav_current_user_principal(char* xml) {
    char* xml_key_pos = strstr(xml, "<D:current-user-principal>");
    // this XML does not contain a user principal at all
    if (xml_key_pos == NULL) {
        return NULL;
    }

    fprintf(stderr, "Found current-user-principal at position: %i\n", *xml_key_pos);

    //<D:current-user-principal>
    //<D:href>/caldav/v2/diary.in0rdr%40gmail.com/user</D:href>
    char* tok = strtok(xml_key_pos, "<"); // D:current-user-principal>
    if (tok != NULL) {
        tok = strtok(NULL, "<"); // D:href>/caldav/v2/test%40gmail.com/user
        fprintf(stderr, "First token: %s\n", tok);
        tok = strstr(tok, ">"); // >/caldav/v2/test%40gmail.com/user
        tok++; // cut >
        char* tok_end = strrchr(tok, '/');
        *tok_end = '\0'; // cut /user
    }

    return tok;
}

// return calendar uri from CalDAV home set XML response
char* parse_caldav_calendar(char* xml, char* calendar) {
    char displayname_needle[strlen(calendar) + strlen("<D:displayname></D:displayname>")];
    sprintf(displayname_needle, "<D:displayname>%s</D:displayname>", calendar);
    fprintf(stderr, "Displayname needle: %s\n", displayname_needle);
    char* displayname_pos = strstr(xml, displayname_needle);
    // this XML multistatus response does not contain the users calendar
    if (displayname_pos == NULL) {
        return NULL;
    }

    // <D:response>
    //  <D:href>/caldav/v2/2fcv7j5mf38o5u2kg5tar4baao%40group.calendar.google.com/events/</D:href>
    //  <D:propstat>
    //   <D:status></D:status>
    //   <D:prop>
    //    <D:displayname>diary</D:displayname>
    //   </D:prop>
    //  </D:propstat>
    // </D:response>

    // shorten multistatus response and find last hyperlink
    *displayname_pos= '\0';
    char* href = strrstr(xml, "<D:href>");
    if (href != NULL) {
        fprintf(stderr, "Found calendar href: %s\n", href);
        href = strtok(href, "<"); // :href>/caldav/v2/aaa%40group.calendar.google.com/events/
        if (href != NULL) {
            href = strchr(href, '>');
            href++; // cut >
            fprintf(stderr, "Found calendar href: %s\n", href);
        }
        return href;
    }
    return NULL;
}

void put_event(struct tm* date, const char* dir, size_t dir_size, char* calendar_uri) {
    // get entry path
    char path[100];
    char* ppath = path;
    char* descr;
    long descr_bytes;

    fpath(dir, dir_size, date, &ppath, sizeof path);
    if (ppath == NULL) {
        fprintf(stderr, "Error while retrieving file path for diary reading");
        return;
    }

    FILE* fp = fopen(path, "r");
    if (fp == NULL) perror("Error opening file");

    fseek(fp, 0, SEEK_END);
    descr_bytes = ftell(fp);
    rewind(fp);

    size_t descr_labell = strlen("DESCRIPTION:");
    size_t descrl = descr_bytes + descr_labell + 1;
    descr = malloc(descrl);
    if (descr == NULL) {
        perror("malloc failed");
        return;
    }

    descr[0] = '\0';
    strcat(descr, "DESCRIPTION:");

    int items_read = fread(descr + descr_labell, sizeof(char), descr_bytes, fp);
    if (items_read != descr_bytes) {
        fprintf(stderr, "Read %i items but expected %li, aborting.", items_read, descr_bytes);
        return;
    }

    descr[descrl] = '\0';
    fprintf(stderr, "File buffer that will be uploaded to the remote CalDAV server:\n%s\n", descr);

    char* folded_descr = fold(descr);
    fprintf(stderr, "Folded descr:%s\n", folded_descr);

    char uid[9];
    strftime(uid, sizeof uid, "%Y%m%d", date);

    char* ics = "BEGIN:VCALENDAR\n"
                "BEGIN:VEVENT\n"
                "UID:%s\n"
                "DTSTART;VALUE=DATE:%s\n"
                "SUMMARY:%s\n"
                "%s\n"
                "END:VEVENT\n"
                "END:VCALENDAR";
    char postfields[strlen(ics) + strlen(folded_descr) + 100];
    sprintf(postfields, ics,
            uid,
            uid,
            uid, // todo: display first few chars of DESCRIPTION as SUMMARY
            folded_descr);

    fprintf(stderr, "PUT event postfields:\n%s\n", postfields);

    strcat(calendar_uri, uid);
    strcat(calendar_uri, ".ics");
    fprintf(stderr, "Event uri:\n%s\n", calendar_uri);
    char* response = caldav_req(date, calendar_uri, "PUT", postfields, 0);
    fprintf(stderr, "PUT event response:\n%s\n", response);
    fclose(fp);
    free(folded_descr);
    free(descr);

    if (response == NULL) {
        fprintf(stderr, "PUT event failed.\n");
    }
}

/*
* Sync with CalDAV server.
* Returns the answer char of the confirmation dialogue
* Returns 0 if neither local nor remote file exists.
* Otherwise, returns -1 on error.
*/
int caldav_sync(struct tm* date,
                 WINDOW* header,
                 WINDOW* cal,
                 int pad_pos,
                 const char* dir,
                 size_t dir_size,
                 bool confirm) {
    pthread_t progress_tid;

    // fetch existing API tokens
    char* tokfile = read_tokenfile();
    free(tokfile);

    if (access_token == NULL && refresh_token == NULL) {
        // no access token exists yet, create new verifier
        char challenge[GOOGLE_OAUTH_CODE_VERIFIER_LENGTH];
        random_code_challenge(GOOGLE_OAUTH_CODE_VERIFIER_LENGTH, challenge);
        fprintf(stderr, "Challenge/Verifier: %s\n", challenge);

        // fetch new code with verifier
        char* code = get_oauth_code(challenge, header);
        if (code == NULL) {
            fprintf(stderr, "Error retrieving access code.\n");
            return -1;
        }

        // get acess token using code and verifier
        get_access_token(code, challenge, false);
    }

    pthread_create(&progress_tid, NULL, show_progress, (void*)header);
    pthread_detach(progress_tid);

    char* principal_postfields = "<d:propfind xmlns:d='DAV:' xmlns:cs='http://calendarserver.org/ns/'>"
                                 "<d:prop><d:current-user-principal/></d:prop>"
                                 "</d:propfind>";


    // check if we can use the token from the tokenfile
    char* user_principal = caldav_req(date, GOOGLE_CALDAV_URI, "PROPFIND", principal_postfields, 0);
    fprintf(stderr, "User principal: %s\n", user_principal);

    if (user_principal == NULL) {
        fprintf(stderr, "Unable to fetch principal, refreshing API token.\n");
        // The principal could not be fetched,
        // get new acess token with refresh token
        get_access_token(NULL, NULL, true);
        // Retry request for event with new token
        user_principal = caldav_req(date, GOOGLE_CALDAV_URI, "PROPFIND", principal_postfields, 0);
    }

    if (user_principal == NULL) {
        pthread_cancel(progress_tid);
        wclear(header);
        wresize(header, LINES, getmaxx(header));
        char* info_txt = "Offline or invalid Google OAuth2 credentials.\n"
                         "Go online or delete tokenfile '%s' to retry login.\n"
                         "Press any key to continue.";
        mvwprintw(header, 0, 0, info_txt, CONFIG.google_tokenfile);
        wrefresh(header);

        // accept any input to proceed
        noecho();
        wgetch(header);
        echo();

        free(access_token);
        access_token = NULL;
        free(refresh_token);
        refresh_token = NULL;
        pthread_cancel(progress_tid);
        wclear(header);
        return -1;
    }

    user_principal = parse_caldav_current_user_principal(user_principal);
    fprintf(stderr, "\nUser Principal: %s\n", user_principal);

    // get the home-set of the user
    char uri[300];
    sprintf(uri, "%s%s", GOOGLE_API_URI, user_principal);
    fprintf(stderr, "\nHome Set URI: %s\n", uri);
    char* home_set = caldav_req(date, uri, "PROPFIND", "", 1);
    fprintf(stderr, "\nHome Set: %s\n", home_set);

    // get calendar URI from the home-set
    char* calendar_href = parse_caldav_calendar(home_set, CONFIG.google_calendar);
    fprintf(stderr, "\nCalendar href: %s\n", calendar_href);

    char* xml_filter = "<c:calendar-query xmlns:d='DAV:' xmlns:c='urn:ietf:params:xml:ns:caldav'>"
                       "<d:prop><c:calendar-data/></d:prop>"
                       "<c:filter><c:comp-filter name='VCALENDAR'>"
                       "<c:comp-filter name='VEVENT'>"
                       "<c:time-range start='%s' end='%s'/></c:comp-filter>"
                       "</c:comp-filter></c:filter></c:calendar-query>";

    // construct next day from date+1
    time_t date_time = mktime(date);
    struct tm* next_day = localtime(&date_time);
    next_day->tm_mday++;
    mktime(next_day);

    char dstr_cursor[30];
    char dstr_next_day[30];

    char* format = "%Y%m%dT000000Z";
    strftime(dstr_cursor, sizeof dstr_cursor, format, date);
    strftime(dstr_next_day, sizeof dstr_next_day, format, next_day);

    char caldata_postfields[strlen(xml_filter)+50];
    sprintf(caldata_postfields, xml_filter,
            dstr_cursor,
            dstr_next_day);
    fprintf(stderr, "Calendar data postfields:\n%s\n", caldata_postfields);

    // fetch event for the cursor date
    sprintf(uri, "%s%s", GOOGLE_API_URI, calendar_href);
    fprintf(stderr, "\nCalendar URI: %s\n", uri);
    char* event = caldav_req(date, uri, "REPORT", caldata_postfields, 0);
    fprintf(stderr, "Event:\n%s", event);
    // todo: warn if multiple events,
    // multistatus has more than just one caldav:calendar-data elements

    // get path of entry
    char path[100];
    char* ppath = path;
    fpath(CONFIG.dir, strlen(CONFIG.dir), date, &ppath, sizeof path);
    fprintf(stderr, "Cursor date file path: %s\n", path);

    bool local_file_exists = true;
    bool remote_file_exists = true;

    // check last modification time of local time
    struct stat attr;
    if (stat(path, &attr) != 0) {
        perror("Stat failed");
        local_file_exists = false;
    }
    struct tm* localfile_time = gmtime(&attr.st_mtime);
    fprintf(stderr, "Local dst: %i\n", localfile_time->tm_isdst);
    // set to negative value, so mktime uses timezone information and system databases
    // to attempt to determine whether DST is in effect at the specified time
    localfile_time->tm_isdst = -1;
    time_t localfile_date = mktime(localfile_time);
    fprintf(stderr, "Local dst: %i\n", localfile_time->tm_isdst);
    fprintf(stderr, "Local file last modified time: %s\n", ctime(&localfile_date));
    fprintf(stderr, "Local file last modified time: %s\n", ctime(&attr.st_mtime));

    struct tm remote_datetime;
    time_t remote_date;
    long search_pos = 0;

    // check remote LAST-MODIFIED:20210521T212441Z of remote event
    char* remote_last_mod = extract_ical_field(event, "LAST-MODIFIED", &search_pos, false);
    char* remote_uid = extract_ical_field(event, "UID", &search_pos, false);

    if (remote_last_mod == NULL || remote_uid == NULL) {
        remote_file_exists = false;
    } else {
        fprintf(stderr, "Remote last modified: %s\n", remote_last_mod);
        fprintf(stderr, "Remote UID: %s\n", remote_uid);
        strptime(remote_last_mod, "%Y%m%dT%H%M%SZ", &remote_datetime);
        //remote_datetime.tm_isdst = -1;
        fprintf(stderr, "Remote dst: %i\n", remote_datetime.tm_isdst);
        remote_date = mktime(&remote_datetime);
        fprintf(stderr, "Remote dst: %i\n", remote_datetime.tm_isdst);
        fprintf(stderr, "Remote last modified: %s\n", ctime(&remote_date));
        free(remote_last_mod);
    }

    if (! (local_file_exists || remote_file_exists)) {
        fprintf(stderr, "Neither local nor remote file exists, giving up.\n");
        pthread_cancel(progress_tid);
        wclear(header);
        return 0;
    }

    double timediff = difftime(localfile_date, remote_date);
    fprintf(stderr, "Time diff between local and remote mod time:%e\n", timediff);

    if (local_file_exists && (timediff > 0 || !remote_file_exists)) {
        // local time > remote time
        // if local file mod time more recent than LAST-MODIFIED

        if (remote_file_exists) {
            // purge any existing daily calendar entries on the remote side
            char event_uri[300];
            sprintf(event_uri, "%s%s%s.ics", GOOGLE_API_URI, calendar_href, remote_uid);

            caldav_req(date, event_uri, "DELETE", "", 0);
        }

        fputs("Local file is newer, uploading to remote...\n", stderr);
        put_event(date, dir, dir_size, uri);

        pthread_cancel(progress_tid);
        wclear(header);

    }

    char* rmt_desc;
    char dstr[16];
    int conf_ch;
    if (remote_file_exists && (timediff < 0 || !local_file_exists)) {
        rmt_desc = extract_ical_field(event, "DESCRIPTION", &search_pos, true);
        fprintf(stderr, "Remote event description:%s\n", rmt_desc);

        if (rmt_desc == NULL) {
            fprintf(stderr, "Could not fetch description of remote event.\n");
            pthread_cancel(progress_tid);
            wclear(header);
            return -1;
        }

        if (confirm) {
            // prepare header for confirmation dialogue
            curs_set(2);
            noecho();
            pthread_cancel(progress_tid);
            wclear(header);
        }

        // ask for confirmation
        strftime(dstr, sizeof dstr, CONFIG.fmt, date);
        mvwprintw(header, 0, 0, "Remote event is more recent. Sync entry '%s' and overwrite local file? [(Y)es/(a)ll/(n)o/(c)ancel] ", dstr);
        char* i;
        bool conf = false;
        while (!conf) {
            conf_ch = wgetch(header);
            if (conf_ch == 'y' || conf_ch == 'Y' || 'a' || conf_ch == '\n' || !confirm) {
                fprintf(stderr, "Remote file is newer, extracting description from remote...\n");

                // persist downloaded buffer to local file
                FILE* cursordate_file = fopen(path, "wb");
                if (cursordate_file == NULL) {
                    perror("Failed to open cursor date file");
                } else {
                    for (i = rmt_desc; *i != '\0'; i++) {
                        if (rmt_desc[i-rmt_desc] == 0x5C) { // backslash
                            switch (*(i+1)) {
                                case 'n':
                                    fputc('\n', cursordate_file);
                                    i++;
                                    break;
                                case 0x5c: // preserve real backslash
                                    fputc(0x5c, cursordate_file);
                                    i++;
                                    break;
                            }
                        } else {
                            fputc(*i, cursordate_file);
                        }
                    }
                }
                fclose(cursordate_file);

                // add new entry highlight
                chtype atrs = winch(cal) & A_ATTRIBUTES;
                wchgat(cal, 2, atrs | A_BOLD, 0, NULL);
                prefresh(cal, pad_pos, 0, 1, ASIDE_WIDTH, LINES - 1, ASIDE_WIDTH + CAL_WIDTH);
            }
            break;
        }

        echo();
        curs_set(0);
        free(rmt_desc);
    }
    return conf_ch;
}
