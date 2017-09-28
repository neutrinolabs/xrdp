#include "utils.h"

Utils::Utils()
{
}

int Utils::getMountList(void *wtsChannel, QList<QListWidgetItem *> *itemList)
{
    QListWidgetItem *item;

    STREAM   s;
    quint32  bytesToSend;
    quint32  bytesWritten;
    quint32  bytesRead;
    quint32  cmdLen;
    quint32  cmd;
    int      rv;
    int      i;
    int      nentries;
    char     buf[2048];

    if (!wtsChannel)
        return -1;

    qstream_new(s, 1024 * 8);

    /*
     * command format:
     *  4 bytes   cmd_len  length of this command
     *  1 byte    cmd      TCU_CMD_GET_MOUNT_LIST
     */

    /* setup command */
    qstream_wr_u32(&s, 1);
    qstream_wr_u8(&s, TCU_CMD_GET_MOUNT_LIST);
    bytesToSend = 5;

    /* send command */
    rv = WTSVirtualChannelWrite(wtsChannel, s.data, bytesToSend, &bytesWritten);
    if (rv == 0)
    {
        QMessageBox::information(NULL, "Get device list", "\nError sending "
                                 "command to client");
        return -1;
    }

    qstream_set_pos(&s, 0); /* reuse stream */

    /* get response */

    /*
     * response format
     * 	4 bytes  cmd_len     length of this command
     * 	4 bytes  cmd         TCU_CMD_GET_MOUNT_LIST
     * 	1 byte   nentries    number of entries in this pkt
     * 	n bytes  entry_list  nentries null terminated strings
     */

    rv = WTSVirtualChannelRead(wtsChannel, 1000 * 5, s.data, 1024 * 8, &bytesRead);
    if (rv == 0 || bytesRead == 0)
        goto error;

    /* did we get the entire cmd? */
    qstream_rd_u32(&s, cmdLen);
    if (cmdLen != bytesRead - 4)
        goto error;

    /* did we get the right response? */
    qstream_rd_u32(&s, cmd);
    if (cmd != TCU_CMD_GET_MOUNT_LIST)
        goto error;

    qstream_rd_u8(&s, nentries);
    if (nentries == 0)
        goto done;

    for (i = 0; i < nentries; i++)
    {
        strcpy(buf, s.pos);
        qstream_inc_pos(&s, strlen(buf) + 1);
        item = new QListWidgetItem;
        item->setText(QString(buf));
        itemList->append(item);
    }

done:
    qstream_free(&s);
    return 0;

error:
    qstream_free(&s);
    return -1;
}

int Utils::unmountDevice(void *wtsChannel, QString device, QStatusBar *statusBar)
{
    STREAM      s;
    quint32     bytesRead;
    quint32     cmdLen;
    quint32     cmd;
    quint32     bytesWritten;
    int         bytesToSend;
    int         rv;
    int         seconds = 0;
    char        status;
    char*       buf;

    if (!wtsChannel)
        return -1;

    qstream_new(s, 1024);
    buf = device.toAscii().data();

    /*
     * command format:
     *  4 bytes  cmd_len  length of this command
     *  4 bytes  cmd      TCU_CMD_UNMOUNT_DEVICE
     *  n bytes  device   null terminated device name
     */

    /* setup command */
    bytesToSend = 4 + 4 + device.count() + 1;
    qstream_wr_u32(&s, bytesToSend - 4);
    qstream_wr_u32(&s, TCU_CMD_UNMOUNT_DEVICE);
    strcpy(s.pos, buf);

    /* send command */
    rv = WTSVirtualChannelWrite(wtsChannel, s.data, bytesToSend, &bytesWritten);
    if (rv == 0)
    {
        QMessageBox::information(NULL, "Unmount device", "\nError sending "
                                 "command to client");
        return -1;
    }

    qstream_set_pos(&s, 0); /* reuse stream */

    /* get response */

    /*
     * command format
     *      4 bytes  cmd_len     number of bytes in this pkt
     *      4 bytes  cmd         TCU_CMD_UNMOUNT_DEVICE
     *      1 byte   status      operation status code
     */

    while (1)
    {
        rv = WTSVirtualChannelRead(wtsChannel, 1000 * 1, s.data, 1024, &bytesRead);
        if (rv == 0)
            goto error;

        if (bytesRead)
           break;

        statusBar->showMessage("Waiting for client to unmount device: " +
                               QString::number(++seconds) + " seconds", 30000);
    }

    statusBar->showMessage("");

    /* did we get the entire pkt? */
    qstream_rd_u32(&s, cmdLen);
    if (cmdLen != bytesRead - 4)
        goto error;

    /* did we get the right response? */
    qstream_rd_u32(&s, cmd);
    if (cmd != TCU_CMD_UNMOUNT_DEVICE)
        goto error;

    /* get status */
    qstream_rd_u8(&s, status);
    switch(status)
    {
    case UMOUNT_SUCCESS:
        goto done;
        break;

    case UMOUNT_BUSY:
        QMessageBox::information(NULL, "Unmount error", "\nCannot unmount device "
                                 "because it is in use");
        break;

    case UMOUNT_NOT_MOUNTED:
        QMessageBox::information(NULL, "Unmount error", "\nCannot unmount device "
                                 "because it is not mounted");
        break;

    case UMOUNT_OP_NOT_PERMITTED:
        QMessageBox::information(NULL, "Unmount error", "\nOperation not "
                                 "permitted. The device may be in use");
        break;

    case UMOUNT_PERMISSION_DENIED:
        QMessageBox::information(NULL, "Unmount error", "\nYou don't have "
                                 "permission to unmount this device");
        break;

    case UMOUNT_UNKNOWN:
        QMessageBox::information(NULL, "Unmount error", "Cannot unmount "
                                 "device, an unknown error has occurred. The "
                                 "drive may be in use or you do not have "
                                 "permission to unmount it");
        break;
    }

    goto error;

done:
    qstream_free(&s);
    return 0;

error:
    qstream_free(&s);
    return -1;
}
