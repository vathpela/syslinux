/*
 * TFTP data output backend
 */

#include <string.h>
#include <stdio.h>
#include <syslinux/pxe.h>
#include <syslinux/config.h>
#include <netinet/in.h>
#include <sys/times.h>
#include <fs/pxe/pxe.h>
#include <fs/pxe/url.h>
#include <fs/pxe/tftp.h>
#include "upload_backend.h"

const char *tftp_string_error_message[]={
    "Unknown error",
    "File not found",
    "Access Denied",
    "Disk Full",
    "Illegal Operation",
    "Unknown Transfer ID",
    "File already exists",
    "Unknown User",
    "Negotiation failed",

    /* These are not in any RFC, defined internally */
    "Unable to resolve hostname",
    "Unable to connect",
    "No Error",
    "Network unavailable",
};

static bool have_real_network(void);

static int upload_tftp_write(struct upload_backend *be) {
    const union syslinux_derivative_info *sdi =
	syslinux_derivative_info();
    struct url_info url;
    struct inode inode;
    char url_path[255] = {0};
    uint32_t ip;
    int err;

    if (!have_real_network()) {
	dprintf("\nNot running from the network\n");
	return -TFTP_ERR_NO_NETWORK;
    }

    if (be->argv[1]) {
        ip = pxe_dns(be->argv[1]);
        if (!ip) {
            dprintf("\nUnable to resolve hostname: %s\n", be->argv[1]);
            return -TFTP_ERR_UNABLE_TO_RESOLVE;
        }
    } else {
        ip   = sdi->pxe.ipinfo->serverip;
        if (!ip) {
            dprintf("\nNo server IP address\n");
            return -TFTP_ERR_UNABLE_TO_CONNECT;
        }
    }

    snprintf(url_path, sizeof(url_path), "tftp://%u.%u.%u.%u/%s",
	((uint8_t *)&ip)[0],
	((uint8_t *)&ip)[1],
	((uint8_t *)&ip)[2],
	((uint8_t *)&ip)[3],
	be->argv[0]);

    parse_url(&url, url_path);
    url.ip = ip;

    dprintf("Connecting to %s to send %s\n", url.host, url.path);
    err = tftp_put(&url, 0, &inode, NULL, be->outbuf, be->zbytes);

    if (-err != TFTP_OK)
	printf("upload_tftp_write: TFTP server returned error %d : %s\n", err, tftp_string_error_message[-err]);

    return -err;
}

struct upload_backend upload_tftp = {
    .name       = "tftp",
    .helpmsg    = "filename [tftp_server]",
    .minargs    = 1,
    .write      = upload_tftp_write,
};

/*
 * Dummy functions to prevent link failure for non-network cores
 */
static int _dummy_tftp_put(struct url_info *url, int flags,
			   struct inode *inode, const char **redir,
			   char *data, int data_length)
{
    (void)url;
    (void)flags;
    (void)inode;
    (void)redir;
    (void)data;
    (void)data_length;

    return -TFTP_ERR_NO_NETWORK;
}

__weak int __attribute__((alias("_dummy_tftp_put")))
tftp_put(struct url_info *url, int flags, struct inode *inode,
	 const char **redir, char *data, int data_length);

static int _dummy_tftp_put(struct url_info *url, int flags,
			   struct inode *inode, const char **redir,
			   char *data, int data_length);

static bool have_real_network(void)
{
    return tftp_put != _dummy_tftp_put;
}

__weak uint32_t pxe_dns(const char *host)
{
    (void)host;

    return 0;
}

__weak void parse_url(struct url_info *ui, char *url)
{
    (void)ui;
    (void)url;
}
