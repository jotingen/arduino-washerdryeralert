#include <system.h>

/* ------------------------------------------------- */

#include <ESP_Mail_Client.h>

/* ------------------------------------------------- */

// Provides:
//   char* SMTP_HOST "smtp.gmail.com"
//   SMTP_PORT 465
//   char* AUTHOR_EMAIL "***@gmail.com"
//   char* AUTHOR_PASSWORD "***"
#include "EmailCredentials.h"

/* ------------------------------------------------- */

/* The SMTP Session object used for Email sending */
SMTPSession smtp;

/* ------------------------------------------------- */

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status)
{
  /* Print the current status */
  println(status.info());

  /* Print the sending result */
  if (status.success())
  {
    println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failled: %d\n", status.failedCount());
    println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);

      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients);
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject);
    }
    println("----------------\n");
  }
}

/* ------------------------------------------------- */

void sendMsg(const char *recipient, const char *msg)
{
  /** Enable the debug via Serial port
   * none debug or 0
   * basic debug or 1
   */
  smtp.debug(1);

  /* Set the callback function to get the sending results */
  smtp.callback(smtpCallback);

  /* Declare the session config data */
  ESP_Mail_Session session;

  /* Set the session config */
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "";

  /* Declare the message class */
  SMTP_Message message;

  /* Set the message headers */
  message.sender.name = "Washer Dryer Alert";
  message.sender.email = AUTHOR_EMAIL;
  // message.subject = "Washer Done";
  message.addRecipient("Main", recipient);

  /*Send HTML message*/
  message.html.content = msg;
  message.text.charSet = "us-ascii";
  message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  /* Connect to server with the session config */
  if (!smtp.connect(&session))
    return;

  /* Start sending Email and close the session */
  if (!MailClient.sendMail(&smtp, &message))
    println("Error sending Email, " + smtp.errorReason());
}