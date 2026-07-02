#include <adwaita.h>
#include <webkit/webkit.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

static constexpr const char* USER_AGENT =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/141.0.0.0 Safari/537.36";

struct AppData
{
    std::string service;  // "mail", "calendar", ...
    std::string title;    // "Mail", "Calendar", ...
    std::string tld;      // ".com", ".com.cn", ...
};

static std::string get_data_dir()
{
    const char* xdg = getenv( "XDG_DATA_HOME" );
    std::string base;
    if ( xdg && xdg[0] )
    {
        base = xdg;
    }
    else
    {
        const char* home = getenv( "HOME" );
        base = home ? home : "";
        base += "/.local/share";
    }
    return base + "/icloud-linux";
}

// Read TLD: Flatpak → XDG_DATA_HOME/icloud-linux/tld
static std::string read_tld()
{
    std::string tld = ".com";
    std::string path = get_data_dir() + "/tld";
    if ( std::filesystem::exists( path ) )
    {
        std::ifstream f( path );
        tld = std::string( ( std::istreambuf_iterator<char>( f ) ), {} );
    }
    return tld;
}



static void load_window_geometry( const std::string& service, int& width, int& height, bool& maximized )
{
    width = 1280;
    height = 800;
    maximized = false;

    std::string path = get_data_dir() + "/geometry_" + service;
    if ( std::filesystem::exists( path ) )
    {
        std::ifstream f( path );
        int w, h, m;
        if ( f >> w >> h >> m )
        {
            width = w;
            height = h;
            maximized = ( m != 0 );
        }
    }
}

static void save_window_geometry( const std::string& service, int width, int height, bool maximized )
{
    std::filesystem::path path = get_data_dir() + "/geometry_" + service;
    std::filesystem::create_directories( path.parent_path() );
    std::ofstream f( path );
    if ( f.is_open() )
    {
        f << width << " " << height << " " << ( maximized ? 1 : 0 ) << "\n";
    }
}

struct WindowState
{
    std::string service;
    int last_width;
    int last_height;
    GtkWidget* window;
};

static void window_size_changed_cb( GObject* /* object */, GParamSpec* /* pspec */, gpointer user_data )
{
    auto* ws = static_cast<WindowState*>( user_data );
    if ( ws->window && !gtk_window_is_maximized( GTK_WINDOW( ws->window ) ) )
    {
        ws->last_width = gtk_widget_get_width( ws->window );
        ws->last_height = gtk_widget_get_height( ws->window );
    }
}

static void window_mapped_cb( GtkWidget* widget, gpointer user_data )
{
    GdkSurface* surface = gtk_native_get_surface( GTK_NATIVE( widget ) );
    if ( surface )
    {
        g_signal_connect( surface, "notify::width", G_CALLBACK( window_size_changed_cb ), user_data );
        g_signal_connect( surface, "notify::height", G_CALLBACK( window_size_changed_cb ), user_data );
    }
}

static gboolean on_close_request( GtkWindow* window, gpointer user_data )
{
    auto* ws = static_cast<WindowState*>( user_data );
    bool maximized = gtk_window_is_maximized( window );
    save_window_geometry( ws->service, ws->last_width, ws->last_height, maximized );
    return FALSE;
}

static void on_destroy( GtkWidget* /* object */, gpointer user_data )
{
    auto* ws = static_cast<WindowState*>( user_data );
    delete ws;
}

static void download_conflict_response_cb( AdwAlertDialog* dlg, const char* response, gpointer /* user_data */ )
{
    WebKitDownload* dl = WEBKIT_DOWNLOAD( g_object_get_data( G_OBJECT( dlg ), "download" ) );
    const char* dest = (const char*)g_object_get_data( G_OBJECT( dlg ), "destination" );
    const char* suggested = (const char*)g_object_get_data( G_OBJECT( dlg ), "suggested_filename" );

    if ( g_strcmp0( response, "overwrite" ) == 0 )
    {
        webkit_download_set_allow_overwrite( dl, TRUE );
        webkit_download_set_destination( dl, dest );
    }
    else if ( g_strcmp0( response, "rename" ) == 0 )
    {
        std::filesystem::path file_path( suggested );
        std::string stem = file_path.stem().string();
        std::string ext = file_path.extension().string();
        std::string dir = std::filesystem::path( dest ).parent_path().string();
        
        std::string new_dest = dest;
        int counter = 1;
        while ( std::filesystem::exists( new_dest ) )
        {
            std::string new_name = stem + " (" + std::to_string( counter ) + ")" + ext;
            new_dest = dir + "/" + new_name;
            counter++;
        }
        webkit_download_set_destination( dl, new_dest.c_str() );
    }
    else
    {
        webkit_download_cancel( dl );
    }
    g_object_unref( dl );
}

static gboolean download_decide_destination_cb( WebKitDownload* download, const gchar* suggested_filename, gpointer /* user_data */ )
{
    const gchar* download_dir = g_get_user_special_dir( G_USER_DIRECTORY_DOWNLOAD );
    if ( !download_dir )
    {
        download_dir = g_get_tmp_dir();
    }

    gchar* destination = g_build_filename( download_dir, suggested_filename, NULL );

    if ( g_file_test( destination, G_FILE_TEST_EXISTS ) )
    {
        WebKitWebView* wv = webkit_download_get_web_view( download );
        GtkWindow* parent_win = nullptr;
        if ( wv )
        {
            GtkWidget* toplevel = gtk_widget_get_ancestor( GTK_WIDGET( wv ), GTK_TYPE_WINDOW );
            if ( toplevel ) parent_win = GTK_WINDOW( toplevel );
        }

        AdwDialog* dialog = ADW_DIALOG( adw_alert_dialog_new( "File Already Exists", "A file with this name already exists. What would you like to do?" ) );
        adw_alert_dialog_add_response( ADW_ALERT_DIALOG( dialog ), "cancel", "Cancel" );
        adw_alert_dialog_add_response( ADW_ALERT_DIALOG( dialog ), "rename", "Create Copy" );
        adw_alert_dialog_add_response( ADW_ALERT_DIALOG( dialog ), "overwrite", "Overwrite" );
        
        adw_alert_dialog_set_response_appearance( ADW_ALERT_DIALOG( dialog ), "overwrite", ADW_RESPONSE_DESTRUCTIVE );
        adw_alert_dialog_set_default_response( ADW_ALERT_DIALOG( dialog ), "rename" );
        adw_alert_dialog_set_close_response( ADW_ALERT_DIALOG( dialog ), "cancel" );

        g_object_set_data_full( G_OBJECT( dialog ), "destination", g_strdup( destination ), g_free );
        g_object_set_data_full( G_OBJECT( dialog ), "suggested_filename", g_strdup( suggested_filename ), g_free );
        g_object_set_data( G_OBJECT( dialog ), "download", download );
        g_object_ref( download ); 

        g_signal_connect( dialog, "response", G_CALLBACK( download_conflict_response_cb ), nullptr );

        if ( parent_win )
            adw_dialog_present( ADW_DIALOG( dialog ), GTK_WIDGET( parent_win ) );
        else
            g_warning("Could not present dialog: parent_win is null");

        g_free( destination );
        return TRUE;
    }

    webkit_download_set_destination( download, destination );
    g_free( destination );
    return TRUE;
}

static void show_toast( WebKitDownload* download, const char* message )
{
    WebKitWebView* wv = webkit_download_get_web_view( download );
    if ( !wv ) return;
    
    GtkWidget* overlay = GTK_WIDGET( g_object_get_data( G_OBJECT( wv ), "toast-overlay" ) );
    if ( !overlay ) return;

    AdwToast* toast = adw_toast_new( message );
    adw_toast_set_timeout( toast, 3 );
    adw_toast_overlay_add_toast( ADW_TOAST_OVERLAY( overlay ), toast );
}

static void download_finished_cb( WebKitDownload* download, gpointer /* user_data */ )
{
    show_toast( download, "File download completed." );
}

static void download_failed_cb( WebKitDownload* download, GError* error, gpointer /* user_data */ )
{
    std::string msg = "Download failed: ";
    msg += ( error ? error->message : "Unknown error" );
    show_toast( download, msg.c_str() );
}

static void download_started_cb( WebKitNetworkSession* /* session */, WebKitDownload* download, gpointer /* user_data */ )
{
    show_toast( download, "File download started." );
    g_signal_connect( download, "decide-destination", G_CALLBACK( download_decide_destination_cb ), nullptr );
    g_signal_connect( download, "finished", G_CALLBACK( download_finished_cb ), nullptr );
    g_signal_connect( download, "failed", G_CALLBACK( download_failed_cb ), nullptr );
}



// Store cookies in XDG_DATA_HOME/icloud-linux/cookies.sqlite
static void setup_network_session( WebKitWebView* webview )
{
    WebKitNetworkSession* session = webkit_web_view_get_network_session( webview );
    WebKitCookieManager* cookie_manager = webkit_network_session_get_cookie_manager( session );

    g_signal_connect( session, "download-started", G_CALLBACK( download_started_cb ), nullptr );

    std::string dir = get_data_dir();
    std::filesystem::create_directories( dir );

    std::string cookie_path = dir + "/cookies.sqlite";
    webkit_cookie_manager_set_persistent_storage( cookie_manager, cookie_path.c_str(),
                                                  WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE );
}

static WebKitWebView* make_webview( WebKitWebView* related = nullptr )
{
    WebKitWebView* wv = related ? WEBKIT_WEB_VIEW( g_object_new( WEBKIT_TYPE_WEB_VIEW, "related-view", related, nullptr ) ) :
                                  WEBKIT_WEB_VIEW( webkit_web_view_new() );

    WebKitSettings* settings = webkit_web_view_get_settings( wv );
    webkit_settings_set_user_agent( settings, USER_AGENT );
    webkit_settings_set_javascript_can_access_clipboard( settings, TRUE );

    return wv;
}

static GtkWidget* on_create( WebKitWebView* source_view, WebKitNavigationAction* /* action */, gpointer user_data )
{
    auto* data = static_cast<AppData*>( user_data );

    WebKitWebView* new_view = make_webview( source_view );
    gtk_widget_set_hexpand( GTK_WIDGET( new_view ), TRUE );
    gtk_widget_set_vexpand( GTK_WIDGET( new_view ), TRUE );

    std::string key = data->service + "_popup";
    int width, height;
    bool maximized;
    load_window_geometry( key, width, height, maximized );

    auto* ws = new WindowState{ key, width, height, nullptr };

    GtkWidget* win = adw_window_new();
    ws->window = win;
    gtk_window_set_title( GTK_WINDOW( win ), ( "iCloud " + data->title + " ⧉" ).c_str() );
    gtk_window_set_default_size( GTK_WINDOW( win ), width, height );
    if ( maximized )
    {
        gtk_window_maximize( GTK_WINDOW( win ) );
    }

    GtkWidget* box = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    GtkWidget* header = adw_header_bar_new();
    gtk_box_append( GTK_BOX( box ), header );
    gtk_box_append( GTK_BOX( box ), GTK_WIDGET( new_view ) );

    GtkWidget* toast_overlay = adw_toast_overlay_new();
    adw_toast_overlay_set_child( ADW_TOAST_OVERLAY( toast_overlay ), box );
    g_object_set_data( G_OBJECT( new_view ), "toast-overlay", toast_overlay );

    adw_window_set_content( ADW_WINDOW( win ), toast_overlay );

    g_signal_connect( win, "map", G_CALLBACK( window_mapped_cb ), ws );
    g_signal_connect( win, "close-request", G_CALLBACK( on_close_request ), ws );
    g_signal_connect( win, "destroy", G_CALLBACK( on_destroy ), ws );

    gtk_window_present( GTK_WINDOW( win ) );

    return GTK_WIDGET( new_view );
}

static void on_activate( GtkApplication* app, gpointer user_data )
{
    auto* data = static_cast<AppData*>( user_data );

    WebKitWebView* webview = make_webview();
    setup_network_session( webview );
    g_signal_connect( webview, "create", G_CALLBACK( on_create ), data );

    std::string url = "https://www.icloud" + data->tld + "/" + data->service;
    webkit_web_view_load_uri( webview, url.c_str() );

    gtk_widget_set_hexpand( GTK_WIDGET( webview ), TRUE );
    gtk_widget_set_vexpand( GTK_WIDGET( webview ), TRUE );

    int width, height;
    bool maximized;
    load_window_geometry( data->service, width, height, maximized );

    auto* ws = new WindowState{ data->service, width, height, nullptr };

    GtkWidget* win = adw_application_window_new( app );
    ws->window = win;
    gtk_window_set_title( GTK_WINDOW( win ), ( "iCloud " + data->title ).c_str() );
    gtk_window_set_default_size( GTK_WINDOW( win ), width, height );
    if ( maximized )
    {
        gtk_window_maximize( GTK_WINDOW( win ) );
    }

    GtkWidget* box = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    GtkWidget* header = adw_header_bar_new();
    gtk_box_append( GTK_BOX( box ), header );
    gtk_box_append( GTK_BOX( box ), GTK_WIDGET( webview ) );

    GtkWidget* toast_overlay = adw_toast_overlay_new();
    adw_toast_overlay_set_child( ADW_TOAST_OVERLAY( toast_overlay ), box );
    g_object_set_data( G_OBJECT( webview ), "toast-overlay", toast_overlay );

    adw_application_window_set_content( ADW_APPLICATION_WINDOW( win ), toast_overlay );

    g_signal_connect( win, "map", G_CALLBACK( window_mapped_cb ), ws );
    g_signal_connect( win, "close-request", G_CALLBACK( on_close_request ), ws );
    g_signal_connect( win, "destroy", G_CALLBACK( on_destroy ), ws );

    gtk_window_present( GTK_WINDOW( win ) );
}

int main( int argc, char** argv )
{
    if ( argc < 3 )
    {
        g_printerr( "Usage: %s <service> <title>\n", argv[0] );
        g_printerr( "Example: %s mail Mail\n", argv[0] );
        return 1;
    }

    AppData data;
    data.service = argv[1];
    data.title = argv[2];
    data.tld = read_tld();

    std::string app_id = "io.github.TaylanTatli.iCloud-Linux." + data.title;
    AdwApplication* app = adw_application_new( app_id.c_str(), G_APPLICATION_DEFAULT_FLAGS );

    g_signal_connect( app, "activate", G_CALLBACK( on_activate ), &data );

    int status = g_application_run( G_APPLICATION( app ), 0, nullptr );
    g_object_unref( app );
    return status;
}
