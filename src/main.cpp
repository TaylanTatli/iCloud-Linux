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



static void load_window_geometry( const std::string& service, int& width, int& height, bool& maximized )
{
    width = 1280;
    height = 800;
    maximized = false;

    auto get_path = [&]() -> std::filesystem::path
    {
        const char* xdg = getenv( "XDG_DATA_HOME" );
        std::string base =
            ( xdg && xdg[0] ) ? std::string( xdg ) : std::string( getenv( "HOME" ) ? getenv( "HOME" ) : "" ) + "/.local/share";
        return std::filesystem::path( base ) / "icloud-linux" / ( "geometry_" + service );
    };

    auto path = get_path();
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
    auto get_path = [&]() -> std::filesystem::path
    {
        const char* xdg = getenv( "XDG_DATA_HOME" );
        std::string base =
            ( xdg && xdg[0] ) ? std::string( xdg ) : std::string( getenv( "HOME" ) ? getenv( "HOME" ) : "" ) + "/.local/share";
        return std::filesystem::path( base ) / "icloud-linux" / ( "geometry_" + service );
    };

    auto path = get_path();
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

    adw_window_set_content( ADW_WINDOW( win ), box );

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
    setup_cookies( webview );
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

    adw_application_window_set_content( ADW_APPLICATION_WINDOW( win ), box );

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
