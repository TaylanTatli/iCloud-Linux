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

// Read TLD: Flatpak → XDG_DATA_HOME/icloud-for-linux/tld
static std::string read_tld()
{
    std::string tld = ".com";

    auto get_path = []() -> std::filesystem::path
    {
        const char* xdg = getenv( "XDG_DATA_HOME" );
        std::string base =
            ( xdg && xdg[0] ) ? std::string( xdg ) : std::string( getenv( "HOME" ) ? getenv( "HOME" ) : "" ) + "/.local/share";
        return std::filesystem::path( base ) / "icloud-linux" / "tld";
    };

    auto path = get_path();
    if ( std::filesystem::exists( path ) )
    {
        std::ifstream f( path );
        tld = std::string( ( std::istreambuf_iterator<char>( f ) ), {} );
    }
    return tld;
}

// Store cookies in XDG_DATA_HOME/icloud-for-linux/cookies.sqlite
static void setup_cookies( WebKitWebView* webview )
{
    WebKitNetworkSession* session = webkit_web_view_get_network_session( webview );
    WebKitCookieManager* cookie_manager = webkit_network_session_get_cookie_manager( session );

    const char* xdg = getenv( "XDG_DATA_HOME" );
    std::string base =
        ( xdg && xdg[0] ) ? std::string( xdg ) : std::string( getenv( "HOME" ) ? getenv( "HOME" ) : "" ) + "/.local/share";

    auto dir = std::filesystem::path( base ) / "icloud-linux";
    std::filesystem::create_directories( dir );

    webkit_cookie_manager_set_persistent_storage( cookie_manager, ( dir / "cookies.sqlite" ).c_str(),
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

    GtkWidget* win = adw_window_new();
    gtk_window_set_title( GTK_WINDOW( win ), ( "iCloud " + data->title + " ⧉" ).c_str() );
    gtk_window_set_default_size( GTK_WINDOW( win ), 1000, 600 );

    GtkWidget* box = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    GtkWidget* header = adw_header_bar_new();
    gtk_box_append( GTK_BOX( box ), header );
    gtk_box_append( GTK_BOX( box ), GTK_WIDGET( new_view ) );

    adw_window_set_content( ADW_WINDOW( win ), box );
    gtk_window_present( GTK_WINDOW( win ) );

    return GTK_WIDGET( new_view );
}

static void on_activate( GtkApplication* app, gpointer user_data )
{
    auto* data = static_cast<AppData*>( user_data );

    WebKitWebView* webview = make_webview();
    setup_cookies( webview );
    g_signal_connect( webview, "create", G_CALLBACK( on_create ), data );

    std::string url = "https://www.icloud" + data->tld + "/" + data->service;
    webkit_web_view_load_uri( webview, url.c_str() );

    gtk_widget_set_hexpand( GTK_WIDGET( webview ), TRUE );
    gtk_widget_set_vexpand( GTK_WIDGET( webview ), TRUE );

    GtkWidget* win = adw_application_window_new( app );
    gtk_window_set_title( GTK_WINDOW( win ), ( "iCloud " + data->title ).c_str() );
    gtk_window_set_default_size( GTK_WINDOW( win ), 1000, 600 );

    GtkWidget* box = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    GtkWidget* header = adw_header_bar_new();
    gtk_box_append( GTK_BOX( box ), header );
    gtk_box_append( GTK_BOX( box ), GTK_WIDGET( webview ) );

    adw_application_window_set_content( ADW_APPLICATION_WINDOW( win ), box );
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

    AdwApplication* app = adw_application_new( "io.github.TaylanTatli.iCloud-Linux", G_APPLICATION_DEFAULT_FLAGS );

    g_signal_connect( app, "activate", G_CALLBACK( on_activate ), &data );

    int status = g_application_run( G_APPLICATION( app ), 0, nullptr );
    g_object_unref( app );
    return status;
}
