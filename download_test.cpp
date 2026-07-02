#include <gtk/gtk.h>
#include <webkit/webkit.h>

static void download_decide_destination_cb(WebKitDownload *download, const gchar *suggested_filename, gpointer user_data) {
    const gchar *download_dir = g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD);
    gchar *destination = g_build_filename(download_dir, suggested_filename, NULL);
    gchar *uri = g_filename_to_uri(destination, NULL, NULL);
    
    g_print("Downloading to: %s\n", uri);
    webkit_download_set_destination(download, uri);
    webkit_download_set_allow_overwrite(download, TRUE);
    
    g_free(destination);
    g_free(uri);
}

static void download_finished_cb(WebKitDownload *download, gpointer user_data) {
    g_print("Download finished.\n");
}

static void download_failed_cb(WebKitDownload *download, GError *error, gpointer user_data) {
    g_print("Download failed: %s\n", error->message);
}

static void download_started_cb(WebKitNetworkSession *session, WebKitDownload *download, gpointer user_data) {
    g_print("Download started!\n");
    g_signal_connect(download, "decide-destination", G_CALLBACK(download_decide_destination_cb), user_data);
    g_signal_connect(download, "finished", G_CALLBACK(download_finished_cb), user_data);
    g_signal_connect(download, "failed", G_CALLBACK(download_failed_cb), user_data);
}
