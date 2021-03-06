/**
 * WarfaceBot, a blind XMPP client for Warface (FPS)
 * Copyright (C) 2015, 2016 Levak Borok <levak92@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "def.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __MINGW32__
# include <Winsock.h>
#else
# include <sys/socket.h>
#endif

#include <errno.h>

#include "wb_stream.h"

#ifdef USE_TLS
# define RECV(Fd, Buf, Size) tls_recv((Fd), (Buf), (Size))
# define PERROR(Str, Ret) tls_perror((Str), (Ret))
#else
# define RECV(Fd, Buf, Size) recv((Fd), (Buf), (Size), 0)
# define PERROR(Str, Ret) perror((Str))
#endif

char *read_stream(int fd)
{
    struct stream_hdr hdr;
    uint8_t *hdr_pos = (uint8_t *) &hdr;
    size_t hdr_read = 0;

    do {
        ssize_t size = RECV(fd, hdr_pos, sizeof(hdr) - (hdr_pos - (uint8_t *) &hdr));

        if (size <= 0)
        {
            PERROR("read", size);
            return NULL;
        }

        hdr_read += size;
        hdr_pos += size;
    }
    while (hdr_read < sizeof (hdr));

    if (hdr.magic != STREAM_MAGIC)
    {
        fprintf(stderr, "Bad header: %x\n", hdr.magic);
        return NULL;
    }

    if (hdr.len == 0)
        return NULL;

    uint8_t *msg = calloc(hdr.len + 1, 1);
    uint8_t *curr_pos = msg;
    size_t read_size = 0;

    do {
        ssize_t size = RECV(fd, curr_pos, hdr.len - (curr_pos - msg));

        if (size <= 0)
        {
            free(msg);
            return NULL;
        }

        read_size += size;
        curr_pos += size;
    } while (read_size < hdr.len);


    switch (hdr.se)
    {
        case SE_PLAIN:
        {
#ifdef DEBUG
            printf("<-(%3u/%3u)-- ", (unsigned) read_size, hdr.len);
            printf("\033[1;32m%s\033[0m\n", msg);
#endif
            break;
        }

        case SE_ENCRYPTED:
        {
            crypt_decrypt(msg, hdr.len);
#ifdef DEBUG
            printf("<-(%3u/%3u)== ", (unsigned) read_size, hdr.len);
            printf("\033[1;32m%s\033[0m\n", msg);
#endif
            break;
        }

        case SE_SERVER_KEY:
        {
            char *end = (char *) msg + 3;
            int key = strtol((char *) msg, &end, 10);
#ifdef DEBUG
            printf("<-(%3u/%3u) KEY: %d\n", (unsigned) read_size, hdr.len, key);
#endif
            crypt_init(key);
            free(msg);

            send_stream_ack(fd);

            return read_stream(fd);
        }

        default:
            fprintf(stderr, "Unsupported stream crypt method: %d\n", hdr.se);
            break;
    };

    return (char *) msg;
}
