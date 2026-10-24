/*
 * mgl.c — motor gráfico MGL para E++.
 *
 * Backend X11 + Xft (Linux). Ventanas y widgets dibujados a
 * mano con estilo Tk: IDs enteros, sin callbacks (E++ no los
 * tiene) y eventos por bombeo (polling).
 *
 * Reglas FFI (§49): solo int/float/string/bool/caracter. Los
 * enteros llegan como long long; los strings como const char
 * *. Los strings que devuelve E++ viven en un anillo de
 * buffers estáticos (patrón de color.c): el compilador no
 * los copia ni los libera.
 *
 * Sin display (DISPLAY ausente o inaccesible) todo degrada a
 * devolver 0/false sin abortar.
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrender.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
 * Xft se declara a mano en lugar de incluir X11/Xft/Xft.h:
 * ese cabecero incluye ft2build.h, que vive en
 * /usr/include/freetype2 y el paso `external` de rivo no
 * añade esa ruta. La ABI de Xft es estable (funciones C
 * sencillas y structs públicas documentadas); se enlaza con
 * -lXft -lfontconfig (mgl.ro documenta el enlace). Del
 * struct XftFont solo se leen ascent y descent, que van
 * al principio del layout público de Xft.h.
 */

typedef struct _XftFont {
    int ascent;
    int descent;
    int height;
    int max_advance_width;
    void *charset;
    void *pattern;
} XftFont;

typedef struct _XftDraw XftDraw;

typedef struct {
    unsigned short width, height;
    short x, y;
    short xOff, yOff;
} MglXGlyphInfo;

typedef struct {
    XRenderColor color;
    unsigned long pixel;
} XftColor;

extern XftFont *XftFontOpenName(
    Display *dpy,
    int screen,
    const char *pattern
);

extern void XftFontClose(Display *dpy, XftFont *font);

extern XftDraw *XftDrawCreate(
    Display *dpy,
    Drawable drawable,
    Visual *visual,
    Colormap colormap
);

extern void XftDrawDestroy(XftDraw *draw);

extern void XftDrawStringUtf8(
    XftDraw *draw,
    XftColor *color,
    XftFont *font,
    int x,
    int y,
    const unsigned char *string,
    int len
);

extern void XftTextExtentsUtf8(
    Display *dpy,
    XftFont *font,
    const unsigned char *string,
    int len,
    MglXGlyphInfo *extents
);

extern int XftColorAllocName(
    Display *dpy,
    Visual *visual,
    Colormap colormap,
    const char *name,
    XftColor *result
);

extern void XftColorFree(
    Display *dpy,
    Visual *visual,
    Colormap colormap,
    XftColor *color
);


/* =========================================================
 * CONSTANTES
 * =========================================================
 */

/* Tipos de evento (event_type). */
#define MGL_EV_NINGUNO    0
#define MGL_EV_CLIC       1
#define MGL_EV_TECLA      2
#define MGL_EV_INTRO      3
#define MGL_EV_CASILLA    4
#define MGL_EV_CERRAR     5

/* Kinds de widget. */
#define MGL_VENTANA   1
#define MGL_ETIQUETA  2
#define MGL_BOTON     3
#define MGL_ENTRADA   4
#define MGL_CASILLA   5
#define MGL_MARCO     6

/* Colores por defecto (0xRRGGBB). */
#define MGL_DEF_FONDO_VENTANA  0xECECECU
#define MGL_DEF_TEXTO          0x1A1A1AU
#define MGL_DEF_BOTON          0xD8D8D8U
#define MGL_DEF_ENTRADA        0xFFFFFFU
#define MGL_DEF_BORDE          0x6E6E6EU
#define MGL_DEF_BORDE_FOCO     0x2A6AB0U
#define MGL_DEF_MARCO          0x9A9A9AU

/* Tamaños fijos. */
#define MGL_CASILLA_LADO   16
#define MGL_ALTO_CASILLA   20
#define MGL_PADDING_TEXTO  5

/* Anillo de strings devueltos a E++. */
#define MGL_BUFFERS_STRING  8
#define MGL_TAM_STRING      8192

/* Caché de colores X. */
#define MGL_CACHE_COLOR     128


/* =========================================================
 * ESTADO
 * =========================================================
 */

typedef struct MglWidget {
    int id;
    int kind;
    int ventana;        /* id de la ventana dueña (id propio en ventanas) */
    int x, y, ancho, alto;
    char *texto;        /* copia propia; NULL si no aplica */
    int texto_max;      /* entradas: capacidad del buffer */
    int habilitado;
    int marcada;        /* casillas */
    unsigned int fg;    /* 0xRRGGBB */
    unsigned int bg;

    /* Solo ventanas. */
    Window xwin;
    GC gc;
    XftDraw *draw;
} MglWidget;


typedef struct MglColorCache {
    unsigned int rgb;
    XftColor xft;
    unsigned long pixel;
    int usado;
} MglColorCache;


static struct {
    Display *dpy;
    int screen;
    int conectado;          /* 1 tras el primer intento */
    Visual *visual;
    Colormap colormap;
    XftFont *fuente;
    char patrimonio_fuente[128];
    Atom wm_delete;

    MglWidget *widgets;
    int cantidad;
    int capacidad;
    int siguiente_id;

    int enfocado;           /* id de la entrada con foco, 0 = ninguna */

    int ev_tipo;
    int ev_widget;
    int ev_tecla;
    int ev_x, ev_y;
    char ev_texto[8];

    MglColorCache cache[MGL_CACHE_COLOR];
    int cache_siguiente;

    int registro_atexit;
} mgl = {
    NULL, 0, 0, NULL, 0, NULL, "sans-12", 0,
    NULL, 0, 0, 1,
    0,
    MGL_EV_NINGUNO, 0, 0, 0, 0, {0},
    {{0}}, 0,
    0
};


static char mgl_buffers[MGL_BUFFERS_STRING][MGL_TAM_STRING];
static int mgl_buffer_indice = 0;


/* =========================================================
 * STRINGS ESTÁTICOS (anillo)
 * =========================================================
 */

static char *mgl_nuevo_buffer(void)
{
    char *b = mgl_buffers[mgl_buffer_indice];

    mgl_buffer_indice =
        (mgl_buffer_indice + 1) % MGL_BUFFERS_STRING;

    b[0] = '\0';

    return b;
}


static void mgl_copiar_string(char *destino, size_t tam, const char *texto)
{
    if (destino == NULL || tam == 0)
        return;

    if (texto == NULL) {
        destino[0] = '\0';
        return;
    }

    snprintf(destino, tam, "%s", texto);
}


static char *mgl_duplicar(const char *texto)
{
    if (texto == NULL)
        texto = "";

    size_t tam = strlen(texto) + 1;

    char *copia = malloc(tam);

    if (copia == NULL)
        return NULL;

    memcpy(copia, texto, tam);

    return copia;
}


/* =========================================================
 * CONEXIÓN Y APAGADO
 * =========================================================
 */

static int mgl_contar_ventanas(void);


static void mgl_apagar(void)
{
    for (int i = 0; i < mgl.cantidad; i++) {
        MglWidget *w = &mgl.widgets[i];

        if (w->texto != NULL)
            free(w->texto);

        if (w->kind == MGL_VENTANA) {
            if (w->draw != NULL)
                XftDrawDestroy(w->draw);

            if (w->gc != NULL)
                XFreeGC(mgl.dpy, w->gc);

            if (w->xwin != 0)
                XDestroyWindow(mgl.dpy, w->xwin);
        }
    }

    free(mgl.widgets);
    mgl.widgets = NULL;
    mgl.cantidad = 0;
    mgl.capacidad = 0;

    for (int i = 0; i < MGL_CACHE_COLOR; i++) {
        if (mgl.cache[i].usado)
            XftColorFree(
                mgl.dpy,
                mgl.visual,
                mgl.colormap,
                &mgl.cache[i].xft
            );
    }

    if (mgl.fuente != NULL)
        XftFontClose(mgl.dpy, mgl.fuente);

    if (mgl.dpy != NULL) {
        XCloseDisplay(mgl.dpy);
        mgl.dpy = NULL;
    }
}


static int mgl_abrir_fuente(void)
{
    if (mgl.fuente != NULL) {
        XftFontClose(mgl.dpy, mgl.fuente);
        mgl.fuente = NULL;
    }

    mgl.fuente = XftFontOpenName(
        mgl.dpy,
        mgl.screen,
        mgl.patrimonio_fuente
    );

    if (mgl.fuente == NULL)
        mgl.fuente = XftFontOpenName(
            mgl.dpy,
            mgl.screen,
            "fixed"
        );

    return mgl.fuente != NULL;
}


static int mgl_conectar(void)
{
    if (mgl.conectado)
        return mgl.dpy != NULL;

    mgl.conectado = 1;

    mgl.dpy = XOpenDisplay(NULL);

    if (mgl.dpy == NULL)
        return 0;

    mgl.screen = DefaultScreen(mgl.dpy);
    mgl.visual = DefaultVisual(mgl.dpy, mgl.screen);
    mgl.colormap = DefaultColormap(mgl.dpy, mgl.screen);

    mgl.wm_delete = XInternAtom(
        mgl.dpy,
        "WM_DELETE_WINDOW",
        False
    );

    if (!mgl_abrir_fuente()) {
        XCloseDisplay(mgl.dpy);
        mgl.dpy = NULL;
        return 0;
    }

    if (!mgl.registro_atexit) {
        atexit(mgl_apagar);
        mgl.registro_atexit = 1;
    }

    return 1;
}


/* =========================================================
 * COLORES
 * =========================================================
 */

static unsigned int mgl_clamp_rgb(unsigned int rgb)
{
    (void) rgb;

    return rgb & 0xFFFFFFU;
}


static unsigned long mgl_pixel(unsigned int rgb)
{
    rgb = mgl_clamp_rgb(rgb);

    for (int i = 0; i < MGL_CACHE_COLOR; i++) {
        if (mgl.cache[i].usado && mgl.cache[i].rgb == rgb)
            return mgl.cache[i].pixel;
    }

    XColor c;
    c.red   = (unsigned short) (((rgb >> 16) & 0xFF) * 257);
    c.green = (unsigned short) (((rgb >> 8) & 0xFF) * 257);
    c.blue  = (unsigned short) ((rgb & 0xFF) * 257);
    c.flags = DoRed | DoGreen | DoBlue;

    if (!XAllocColor(mgl.dpy, mgl.colormap, &c))
        return BlackPixel(mgl.dpy, mgl.screen);

    int i = mgl.cache_siguiente;

    if (mgl.cache[i].usado)
        XftColorFree(
            mgl.dpy,
            mgl.visual,
            mgl.colormap,
            &mgl.cache[i].xft
        );

    mgl.cache[i].rgb = rgb;
    mgl.cache[i].pixel = c.pixel;
    mgl.cache[i].usado = 1;

    mgl.cache_siguiente =
        (i + 1) % MGL_CACHE_COLOR;

    return c.pixel;
}


static XftColor *mgl_xft_color(unsigned int rgb)
{
    rgb = mgl_clamp_rgb(rgb);

    for (int i = 0; i < MGL_CACHE_COLOR; i++) {
        if (mgl.cache[i].usado && mgl.cache[i].rgb == rgb)
            return &mgl.cache[i].xft;
    }

    char nombre[16];

    snprintf(
        nombre,
        sizeof(nombre),
        "#%02X%02X%02X",
        (rgb >> 16) & 0xFF,
        (rgb >> 8) & 0xFF,
        rgb & 0xFF
    );

    int i = mgl.cache_siguiente;

    if (mgl.cache[i].usado)
        XftColorFree(
            mgl.dpy,
            mgl.visual,
            mgl.colormap,
            &mgl.cache[i].xft
        );

    if (!XftColorAllocName(
            mgl.dpy,
            mgl.visual,
            mgl.colormap,
            nombre,
            &mgl.cache[i].xft))
    {
        mgl.cache[i].usado = 0;
        return NULL;
    }

    mgl.cache[i].rgb = rgb;
    mgl.cache[i].usado = 1;

    /* Conserva el pixel para dibujo con GC si hace falta. */
    XColor tmp;
    tmp.red   = (unsigned short) (((rgb >> 16) & 0xFF) * 257);
    tmp.green = (unsigned short) (((rgb >> 8) & 0xFF) * 257);
    tmp.blue  = (unsigned short) ((rgb & 0xFF) * 257);
    tmp.flags = DoRed | DoGreen | DoBlue;

    if (XAllocColor(mgl.dpy, mgl.colormap, &tmp))
        mgl.cache[i].pixel = tmp.pixel;

    mgl.cache_siguiente =
        (i + 1) % MGL_CACHE_COLOR;

    return &mgl.cache[i].xft;
}


/* =========================================================
 * TABLA DE WIDGETS
 * =========================================================
 */

static MglWidget *mgl_buscar(int id)
{
    if (id <= 0)
        return NULL;

    for (int i = 0; i < mgl.cantidad; i++) {
        if (mgl.widgets[i].id == id)
            return &mgl.widgets[i];
    }

    return NULL;
}


static MglWidget *mgl_buscar_ventana(int id)
{
    MglWidget *w = mgl_buscar(id);

    if (w == NULL || w->kind != MGL_VENTANA)
        return NULL;

    return w;
}


static MglWidget *mgl_agregar(int kind, int ventana)
{
    if (mgl.cantidad == mgl.capacidad) {
        int capacidad = mgl.capacidad == 0
            ? 16
            : mgl.capacidad * 2;

        MglWidget *crecida = realloc(
            mgl.widgets,
            sizeof(MglWidget) * (size_t) capacidad
        );

        if (crecida == NULL)
            return NULL;

        mgl.widgets = crecida;
        mgl.capacidad = capacidad;
    }

    MglWidget *w = &mgl.widgets[mgl.cantidad++];

    memset(w, 0, sizeof(MglWidget));

    w->id = mgl.siguiente_id++;
    w->kind = kind;
    w->ventana = ventana;
    w->habilitado = 1;
    w->fg = MGL_DEF_TEXTO;

    return w;
}


static int mgl_contar_ventanas(void)
{
    int n = 0;

    for (int i = 0; i < mgl.cantidad; i++) {
        if (mgl.widgets[i].kind == MGL_VENTANA)
            n++;
    }

    return n;
}


/* =========================================================
 * TEXTO Y MÉTRICAS
 * =========================================================
 */

static void mgl_medir(const char *texto, int *ancho, int *alto)
{
    if (ancho) *ancho = 0;
    if (alto) *alto = 0;

    if (texto == NULL || mgl.fuente == NULL)
        return;

    MglXGlyphInfo extents;

    XftTextExtentsUtf8(
        mgl.dpy,
        mgl.fuente,
        (const unsigned char *) texto,
        (int) strlen(texto),
        &extents
    );

    if (ancho) *ancho = extents.xOff;
    if (alto)
        *alto = mgl.fuente->ascent + mgl.fuente->descent;
}


static void mgl_dibujar_texto(
    MglWidget *ventana,
    int x,
    int y,
    int ancho_caja,
    int alto_caja,
    const char *texto,
    unsigned int rgb
)
{
    if (texto == NULL || texto[0] == '\0')
        return;

    if (ventana == NULL || ventana->draw == NULL || mgl.fuente == NULL)
        return;

    int tw = 0;
    int th = 0;

    mgl_medir(texto, &tw, &th);

    int tx = x + (ancho_caja - tw) / 2;
    int ty = y + (alto_caja - th) / 2 + mgl.fuente->ascent;

    if (tx < x)
        tx = x;

    XftColor *color = mgl_xft_color(rgb);

    if (color == NULL)
        return;

    XftDrawStringUtf8(
        ventana->draw,
        color,
        mgl.fuente,
        tx,
        ty,
        (const unsigned char *) texto,
        (int) strlen(texto)
    );
}


/* =========================================================
 * DIBUJO
 * =========================================================
 */

static void mgl_dibujar_marco(MglWidget *ventana, MglWidget *w)
{
    XSetForeground(
        mgl.dpy,
        ventana->gc,
        mgl_pixel(w->fg)
    );

    XDrawRectangle(
        mgl.dpy,
        ventana->xwin,
        ventana->gc,
        w->x,
        w->y,
        (unsigned) (w->ancho > 0 ? w->ancho - 1 : 0),
        (unsigned) (w->alto > 0 ? w->alto - 1 : 0)
    );
}


static void mgl_dibujar_etiqueta(MglWidget *ventana, MglWidget *w)
{
    mgl_dibujar_texto(
        ventana,
        w->x,
        w->y,
        w->ancho,
        w->alto,
        w->texto,
        w->fg
    );
}


static void mgl_dibujar_boton(MglWidget *ventana, MglWidget *w)
{
    XSetForeground(
        mgl.dpy,
        ventana->gc,
        mgl_pixel(w->habilitado ? w->bg : MGL_DEF_FONDO_VENTANA)
    );

    XFillRectangle(
        mgl.dpy,
        ventana->xwin,
        ventana->gc,
        w->x,
        w->y,
        (unsigned) w->ancho,
        (unsigned) w->alto
    );

    XSetForeground(
        mgl.dpy,
        ventana->gc,
        mgl_pixel(MGL_DEF_BORDE)
    );

    XDrawRectangle(
        mgl.dpy,
        ventana->xwin,
        ventana->gc,
        w->x,
        w->y,
        (unsigned) (w->ancho - 1),
        (unsigned) (w->alto - 1)
    );

    mgl_dibujar_texto(
        ventana,
        w->x,
        w->y,
        w->ancho,
        w->alto,
        w->texto,
        w->habilitado ? w->fg : MGL_DEF_MARCO
    );
}


static void mgl_dibujar_entrada(MglWidget *ventana, MglWidget *w)
{
    XSetForeground(
        mgl.dpy,
        ventana->gc,
        mgl_pixel(w->habilitado ? w->bg : MGL_DEF_FONDO_VENTANA)
    );

    XFillRectangle(
        mgl.dpy,
        ventana->xwin,
        ventana->gc,
        w->x,
        w->y,
        (unsigned) w->ancho,
        (unsigned) w->alto
    );

    int enfocado = mgl.enfocado == w->id && w->habilitado;

    XSetForeground(
        mgl.dpy,
        ventana->gc,
        mgl_pixel(enfocado ? MGL_DEF_BORDE_FOCO : MGL_DEF_BORDE)
    );

    XDrawRectangle(
        mgl.dpy,
        ventana->xwin,
        ventana->gc,
        w->x,
        w->y,
        (unsigned) (w->ancho - 1),
        (unsigned) (w->alto - 1)
    );

    /*
     * Si el texto no cabe, se muestra solo la parte final
     * (lo último escrito).
     */
    const char *texto = w->texto != NULL ? w->texto : "";
    int disponible = w->ancho - 2 * MGL_PADDING_TEXTO;

    if (disponible < 0)
        disponible = 0;

    int tw = 0;

    mgl_medir(texto, &tw, NULL);

    while (texto[0] != '\0' && tw > disponible) {
        texto++;
        mgl_medir(texto, &tw, NULL);
    }

    int th = 0;

    mgl_medir(texto, NULL, &th);

    int tx = w->x + MGL_PADDING_TEXTO;
    int ty = w->y + (w->alto - th) / 2;

    if (mgl.fuente != NULL && ventana->draw != NULL) {
        XftColor *color = mgl_xft_color(w->fg);

        if (color != NULL) {
            XftDrawStringUtf8(
                ventana->draw,
                color,
                mgl.fuente,
                tx,
                ty + mgl.fuente->ascent,
                (const unsigned char *) texto,
                (int) strlen(texto)
            );
        }

        if (enfocado) {
            XSetForeground(
                mgl.dpy,
                ventana->gc,
                mgl_pixel(MGL_DEF_BORDE_FOCO)
            );

            XFillRectangle(
                mgl.dpy,
                ventana->xwin,
                ventana->gc,
                tx + tw + 1,
                w->y + 4,
                1,
                (unsigned) (w->alto - 8)
            );
        }
    }
}


static void mgl_dibujar_casilla(MglWidget *ventana, MglWidget *w)
{
    int lado = MGL_CASILLA_LADO;
    int caja_y = w->y + (MGL_ALTO_CASILLA - lado) / 2;

    XSetForeground(
        mgl.dpy,
        ventana->gc,
        mgl_pixel(w->habilitado ? w->bg : MGL_DEF_FONDO_VENTANA)
    );

    XFillRectangle(
        mgl.dpy,
        ventana->xwin,
        ventana->gc,
        w->x,
        caja_y,
        (unsigned) lado,
        (unsigned) lado
    );

    XSetForeground(
        mgl.dpy,
        ventana->gc,
        mgl_pixel(MGL_DEF_BORDE)
    );

    XDrawRectangle(
        mgl.dpy,
        ventana->xwin,
        ventana->gc,
        w->x,
        caja_y,
        (unsigned) (lado - 1),
        (unsigned) (lado - 1)
    );

    if (w->marcada) {
        XSetForeground(
            mgl.dpy,
            ventana->gc,
            mgl_pixel(w->habilitado ? MGL_DEF_BORDE_FOCO : MGL_DEF_MARCO)
        );

        XDrawLine(
            mgl.dpy,
            ventana->xwin,
            ventana->gc,
            w->x + 3,
            caja_y + lado / 2,
            w->x + lado / 2,
            caja_y + lado - 4
        );

        XDrawLine(
            mgl.dpy,
            ventana->xwin,
            ventana->gc,
            w->x + lado / 2,
            caja_y + lado - 4,
            w->x + lado - 3,
            caja_y + 3
        );
    }

    if (w->texto != NULL) {
        int th = 0;

        mgl_medir(w->texto, NULL, &th);

        mgl_dibujar_texto(
            ventana,
            w->x + lado + 6,
            w->y,
            w->ancho - lado - 6,
            MGL_ALTO_CASILLA,
            w->texto,
            w->habilitado ? w->fg : MGL_DEF_MARCO
        );

        (void) th;
    }
}


static void mgl_redibujar_ventana(MglWidget *ventana)
{
    if (ventana == NULL || ventana->kind != MGL_VENTANA)
        return;

    if (mgl.dpy == NULL)
        return;

    XSetWindowBackground(
        mgl.dpy,
        ventana->xwin,
        mgl_pixel(ventana->bg)
    );

    XClearWindow(mgl.dpy, ventana->xwin);

    for (int i = 0; i < mgl.cantidad; i++) {
        MglWidget *w = &mgl.widgets[i];

        if (w->kind == MGL_VENTANA)
            continue;

        if (w->ventana != ventana->id)
            continue;

        switch (w->kind) {
            case MGL_MARCO:
                mgl_dibujar_marco(ventana, w);
                break;

            case MGL_ETIQUETA:
                mgl_dibujar_etiqueta(ventana, w);
                break;

            case MGL_BOTON:
                mgl_dibujar_boton(ventana, w);
                break;

            case MGL_ENTRADA:
                mgl_dibujar_entrada(ventana, w);
                break;

            case MGL_CASILLA:
                mgl_dibujar_casilla(ventana, w);
                break;

            default:
                break;
        }
    }

    XFlush(mgl.dpy);
}


static void mgl_redibujar_si_ventana(int id)
{
    MglWidget *w = mgl_buscar(id);

    if (w == NULL)
        return;

    mgl_redibujar_ventana(mgl_buscar_ventana(w->ventana));
}


/* =========================================================
 * EVENTOS
 * =========================================================
 */

static void mgl_fijar_evento(
    int tipo,
    int widget,
    const char *texto,
    int tecla,
    int x,
    int y
)
{
    mgl.ev_tipo = tipo;
    mgl.ev_widget = widget;
    mgl.ev_tecla = tecla;
    mgl.ev_x = x;
    mgl.ev_y = y;

    mgl_copiar_string(mgl.ev_texto, sizeof(mgl.ev_texto), texto);
}


static int mgl_significativo(int tipo)
{
    return tipo == MGL_EV_CLIC ||
           tipo == MGL_EV_TECLA ||
           tipo == MGL_EV_INTRO ||
           tipo == MGL_EV_CASILLA ||
           tipo == MGL_EV_CERRAR;
}


static MglWidget *mgl_widget_en(MglWidget *ventana, int x, int y)
{
    /*
     * Se recorre al revés: lo creado después se dibuja
     * encima y debe recibir el clic primero.
     */
    for (int i = mgl.cantidad - 1; i >= 0; i--) {
        MglWidget *w = &mgl.widgets[i];

        if (w->kind == MGL_VENTANA)
            continue;

        if (w->ventana != ventana->id)
            continue;

        if (x < w->x || y < w->y)
            continue;

        if (x >= w->x + w->ancho || y >= w->y + w->alto)
            continue;

        return w;
    }

    return NULL;
}


static void mgl_cerrar_ventana_interna(MglWidget *ventana)
{
    if (ventana == NULL)
        return;

    if (mgl.enfocado > 0) {
        MglWidget *foco = mgl_buscar(mgl.enfocado);

        if (foco != NULL && foco->ventana == ventana->id)
            mgl.enfocado = 0;
    }

    int id = ventana->id;

    if (ventana->draw != NULL) {
        XftDrawDestroy(ventana->draw);
        ventana->draw = NULL;
    }

    if (ventana->gc != NULL) {
        XFreeGC(mgl.dpy, ventana->gc);
        ventana->gc = NULL;
    }

    if (ventana->xwin != 0) {
        XDestroyWindow(mgl.dpy, ventana->xwin);
        ventana->xwin = 0;
    }

    /* Quita la ventana y sus hijos. */
    for (int i = mgl.cantidad - 1; i >= 0; i--) {
        if (mgl.widgets[i].id == id ||
            mgl.widgets[i].ventana == id)
        {
            if (mgl.widgets[i].texto != NULL)
                free(mgl.widgets[i].texto);

            memmove(
                &mgl.widgets[i],
                &mgl.widgets[i + 1],
                sizeof(MglWidget) *
                    (size_t) (mgl.cantidad - i - 1)
            );

            mgl.cantidad--;
        }
    }

    XFlush(mgl.dpy);
}


static void mgl_manejar_clic(MglWidget *ventana, int x, int y)
{
    MglWidget *w = mgl_widget_en(ventana, x, y);

    /* Quitar foco de la entrada anterior. */
    int foco_previo = mgl.enfocado;

    if (w == NULL || w->kind != MGL_ENTRADA)
        mgl.enfocado = 0;

    if (w == NULL || !w->habilitado) {
        if (foco_previo != 0)
            mgl_redibujar_si_ventana(foco_previo);

        mgl_redibujar_ventana(ventana);

        return;
    }

    switch (w->kind) {
        case MGL_BOTON:
            mgl_fijar_evento(MGL_EV_CLIC, w->id, "", 0, x, y);
            break;

        case MGL_ENTRADA:
            mgl.enfocado = w->id;
            XDefineCursor(
                mgl.dpy,
                ventana->xwin,
                XCreateFontCursor(mgl.dpy, XC_xterm)
            );
            mgl_fijar_evento(MGL_EV_NINGUNO, 0, "", 0, 0, 0);
            break;

        case MGL_CASILLA:
            w->marcada = !w->marcada;
            mgl_fijar_evento(
                MGL_EV_CASILLA,
                w->id,
                w->marcada ? "1" : "0",
                0,
                x,
                y
            );
            break;

        default:
            mgl_fijar_evento(MGL_EV_NINGUNO, 0, "", 0, 0, 0);
            break;
    }

    if (foco_previo != 0 && foco_previo != mgl.enfocado)
        mgl_redibujar_si_ventana(foco_previo);

    mgl_redibujar_ventana(ventana);
}


static void mgl_manejar_tecla(XKeyEvent *evento)
{
    char buffer[32];
    KeySym sym = 0;

    int n = XLookupString(
        evento,
        buffer,
        (int) sizeof(buffer) - 1,
        &sym,
        NULL
    );

    MglWidget *entrada =
        mgl.enfocado > 0 ? mgl_buscar(mgl.enfocado) : NULL;

    if (entrada != NULL && entrada->kind == MGL_ENTRADA) {
        if (sym == XK_Return || sym == XK_KP_Enter) {
            mgl_fijar_evento(
                MGL_EV_INTRO,
                entrada->id,
                "",
                (int) sym,
                0,
                0
            );

            return;
        }

        if (sym == XK_BackSpace) {
            size_t largo = entrada->texto != NULL
                ? strlen(entrada->texto)
                : 0;

            if (largo > 0)
                entrada->texto[largo - 1] = '\0';

            mgl_redibujar_ventana(
                mgl_buscar_ventana(entrada->ventana)
            );

            return;
        }
    }

    if (n == 1 && buffer[0] >= 32 && buffer[0] < 127) {
        char texto[2] = { buffer[0], '\0' };

        if (entrada != NULL &&
            entrada->kind == MGL_ENTRADA &&
            entrada->habilitado)
        {
            size_t largo = entrada->texto != NULL
                ? strlen(entrada->texto)
                : 0;

            if (entrada->texto != NULL &&
                (int) largo < entrada->texto_max)
            {
                entrada->texto[largo] = buffer[0];
                entrada->texto[largo + 1] = '\0';
            }

            mgl_redibujar_ventana(
                mgl_buscar_ventana(entrada->ventana)
            );
        }

        mgl_fijar_evento(
            MGL_EV_TECLA,
            entrada != NULL ? entrada->id : 0,
            texto,
            (int) sym,
            0,
            0
        );

        return;
    }

    mgl_fijar_evento(MGL_EV_NINGUNO, 0, "", 0, 0, 0);
}


static void mgl_manejar_evento(XEvent *evento)
{
    switch (evento->type) {

        case Expose: {
            if (evento->xexpose.count != 0)
                break;

            Window xwin = evento->xexpose.window;

            for (int i = 0; i < mgl.cantidad; i++) {
                MglWidget *w = &mgl.widgets[i];

                if (w->kind == MGL_VENTANA && w->xwin == xwin)
                    mgl_redibujar_ventana(w);
            }

            break;
        }

        case ButtonPress: {
            if (evento->xbutton.button != Button1)
                break;

            Window xwin = evento->xbutton.window;

            for (int i = 0; i < mgl.cantidad; i++) {
                MglWidget *w = &mgl.widgets[i];

                if (w->kind == MGL_VENTANA && w->xwin == xwin) {
                    mgl_manejar_clic(
                        w,
                        evento->xbutton.x,
                        evento->xbutton.y
                    );
                    break;
                }
            }

            break;
        }

        case KeyPress: {
            Window xwin = evento->xkey.window;

            for (int i = 0; i < mgl.cantidad; i++) {
                MglWidget *w = &mgl.widgets[i];

                if (w->kind == MGL_VENTANA && w->xwin == xwin) {
                    mgl_manejar_tecla(&evento->xkey);
                    break;
                }
            }

            break;
        }

        case ClientMessage: {
            if ((Atom) evento->xclient.data.l[0] != mgl.wm_delete)
                break;

            Window xwin = evento->xclient.window;

            for (int i = 0; i < mgl.cantidad; i++) {
                MglWidget *w = &mgl.widgets[i];

                if (w->kind == MGL_VENTANA && w->xwin == xwin) {
                    int id = w->id;

                    mgl_cerrar_ventana_interna(w);

                    mgl_fijar_evento(
                        MGL_EV_CERRAR,
                        id,
                        "",
                        0,
                        0,
                        0
                    );

                    break;
                }
            }

            break;
        }

        case ConfigureNotify: {
            Window xwin = evento->xconfigure.window;

            for (int i = 0; i < mgl.cantidad; i++) {
                MglWidget *w = &mgl.widgets[i];

                if (w->kind == MGL_VENTANA && w->xwin == xwin)
                    mgl_redibujar_ventana(w);
            }

            break;
        }

        default:
            break;
    }
}


/*
 * Bombea la cola de X. Con block = 0 procesa todo lo
 * pendiente; con block = 1 espera hasta un evento
 * significativo (los de redibujado se absorben).
 *
 * Devuelve el número de ventanas abiertas tras bombar.
 */
static long long mgl_bombar(int block)
{
    if (mgl.dpy == NULL)
        return 0;

    mgl_fijar_evento(MGL_EV_NINGUNO, 0, "", 0, 0, 0);

    if (block) {
        int hubo_significativo = 0;

        while (!hubo_significativo) {
            if (mgl_contar_ventanas() == 0)
                return 0;

            XEvent evento;

            XNextEvent(mgl.dpy, &evento);

            mgl_fijar_evento(MGL_EV_NINGUNO, 0, "", 0, 0, 0);

            mgl_manejar_evento(&evento);

            hubo_significativo =
                mgl_significativo(mgl.ev_tipo);
        }
    }
    else {
        while (XPending(mgl.dpy) > 0) {
            XEvent evento;

            XNextEvent(mgl.dpy, &evento);

            mgl_manejar_evento(&evento);
        }
    }

    return (long long) mgl_contar_ventanas();
}


/* =========================================================
 * API PARA E++ (FFI)
 * =========================================================
 */

long long mgl_available(void)
{
    return mgl_conectar() ? 1 : 0;
}


long long mgl_window(
    const char *titulo,
    long long ancho,
    long long alto
)
{
    if (!mgl_conectar())
        return 0;

    if (ancho < 1) ancho = 1;
    if (alto < 1) alto = 1;

    MglWidget *ventana = mgl_agregar(MGL_VENTANA, 0);

    if (ventana == NULL)
        return 0;

    Window padre = RootWindow(mgl.dpy, mgl.screen);

    ventana->xwin = XCreateSimpleWindow(
        mgl.dpy,
        padre,
        0,
        0,
        (unsigned) ancho,
        (unsigned) alto,
        0,
        BlackPixel(mgl.dpy, mgl.screen),
        mgl_pixel(MGL_DEF_FONDO_VENTANA)
    );

    if (ventana->xwin == 0) {
        mgl.cantidad--;
        return 0;
    }

    ventana->ancho = (int) ancho;
    ventana->alto = (int) alto;
    ventana->bg = MGL_DEF_FONDO_VENTANA;
    ventana->ventana = ventana->id;

    XStoreName(
        mgl.dpy,
        ventana->xwin,
        titulo != NULL ? titulo : ""
    );

    XSelectInput(
        mgl.dpy,
        ventana->xwin,
        ExposureMask |
        ButtonPressMask |
        KeyPressMask |
        StructureNotifyMask
    );

    XSetWMProtocols(
        mgl.dpy,
        ventana->xwin,
        &mgl.wm_delete,
        1
    );

    ventana->gc = XCreateGC(
        mgl.dpy,
        ventana->xwin,
        0,
        NULL
    );

    ventana->draw = XftDrawCreate(
        mgl.dpy,
        ventana->xwin,
        mgl.visual,
        mgl.colormap
    );

    if (ventana->gc == NULL || ventana->draw == NULL) {
        mgl_cerrar_ventana_interna(ventana);
        return 0;
    }

    XMapRaised(mgl.dpy, ventana->xwin);
    XFlush(mgl.dpy);

    return (long long) ventana->id;
}


void mgl_background(
    long long ventana,
    long long r,
    long long g,
    long long b
)
{
    MglWidget *v = mgl_buscar_ventana((int) ventana);

    if (v == NULL)
        return;

    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;

    v->bg = ((unsigned) r << 16) |
            ((unsigned) g << 8) |
            (unsigned) b;

    mgl_redibujar_ventana(v);
}


long long mgl_destroy(long long ventana)
{
    MglWidget *v = mgl_buscar_ventana((int) ventana);

    if (v == NULL)
        return 0;

    mgl_cerrar_ventana_interna(v);

    return 1;
}


long long mgl_open_windows(void)
{
    return (long long) mgl_contar_ventanas();
}


long long mgl_label(
    long long ventana,
    const char *texto,
    long long x,
    long long y
)
{
    MglWidget *v = mgl_buscar_ventana((int) ventana);

    if (v == NULL)
        return 0;

    MglWidget *w = mgl_agregar(MGL_ETIQUETA, v->id);

    if (w == NULL)
        return 0;

    w->texto = mgl_duplicar(texto);

    if (w->texto == NULL) {
        mgl.cantidad--;
        return 0;
    }

    int tw = 0;
    int th = 0;

    mgl_medir(w->texto, &tw, &th);

    w->x = (int) x;
    w->y = (int) y;
    w->ancho = tw > 0 ? tw : 1;
    w->alto = th > 0 ? th : 1;
    w->fg = MGL_DEF_TEXTO;

    mgl_redibujar_ventana(v);

    return (long long) w->id;
}


long long mgl_button(
    long long ventana,
    const char *texto,
    long long x,
    long long y,
    long long ancho,
    long long alto
)
{
    MglWidget *v = mgl_buscar_ventana((int) ventana);

    if (v == NULL)
        return 0;

    if (ancho < 1) ancho = 1;
    if (alto < 1) alto = 1;

    MglWidget *w = mgl_agregar(MGL_BOTON, v->id);

    if (w == NULL)
        return 0;

    w->texto = mgl_duplicar(texto);

    if (w->texto == NULL) {
        mgl.cantidad--;
        return 0;
    }

    w->x = (int) x;
    w->y = (int) y;
    w->ancho = (int) ancho;
    w->alto = (int) alto;
    w->fg = MGL_DEF_TEXTO;
    w->bg = MGL_DEF_BOTON;

    mgl_redibujar_ventana(v);

    return (long long) w->id;
}


long long mgl_entry(
    long long ventana,
    long long x,
    long long y,
    long long ancho,
    long long alto,
    long long largo_max
)
{
    MglWidget *v = mgl_buscar_ventana((int) ventana);

    if (v == NULL)
        return 0;

    if (ancho < 1) ancho = 1;
    if (alto < 1) alto = 1;
    if (largo_max < 1) largo_max = 64;

    MglWidget *w = mgl_agregar(MGL_ENTRADA, v->id);

    if (w == NULL)
        return 0;

    w->texto_max = (int) largo_max;

    w->texto = malloc((size_t) largo_max + 1);

    if (w->texto == NULL) {
        mgl.cantidad--;
        return 0;
    }

    w->texto[0] = '\0';

    w->x = (int) x;
    w->y = (int) y;
    w->ancho = (int) ancho;
    w->alto = (int) alto;
    w->fg = 0x000000U;
    w->bg = MGL_DEF_ENTRADA;

    mgl_redibujar_ventana(v);

    return (long long) w->id;
}


long long mgl_checkbutton(
    long long ventana,
    const char *texto,
    long long x,
    long long y
)
{
    MglWidget *v = mgl_buscar_ventana((int) ventana);

    if (v == NULL)
        return 0;

    MglWidget *w = mgl_agregar(MGL_CASILLA, v->id);

    if (w == NULL)
        return 0;

    w->texto = mgl_duplicar(texto);

    if (w->texto == NULL) {
        mgl.cantidad--;
        return 0;
    }

    int tw = 0;

    mgl_medir(w->texto, &tw, NULL);

    w->x = (int) x;
    w->y = (int) y;
    w->ancho = MGL_CASILLA_LADO + 6 + (tw > 0 ? tw : 1);
    w->alto = MGL_ALTO_CASILLA;
    w->fg = MGL_DEF_TEXTO;
    w->bg = MGL_DEF_ENTRADA;

    mgl_redibujar_ventana(v);

    return (long long) w->id;
}


long long mgl_frame(
    long long ventana,
    long long x,
    long long y,
    long long ancho,
    long long alto
)
{
    MglWidget *v = mgl_buscar_ventana((int) ventana);

    if (v == NULL)
        return 0;

    if (ancho < 1) ancho = 1;
    if (alto < 1) alto = 1;

    MglWidget *w = mgl_agregar(MGL_MARCO, v->id);

    if (w == NULL)
        return 0;

    w->x = (int) x;
    w->y = (int) y;
    w->ancho = (int) ancho;
    w->alto = (int) alto;
    w->fg = MGL_DEF_MARCO;

    mgl_redibujar_ventana(v);

    return (long long) w->id;
}


const char *mgl_get(long long id)
{
    MglWidget *w = mgl_buscar((int) id);

    char *b = mgl_nuevo_buffer();

    if (w == NULL)
        return b;

    if (w->kind == MGL_CASILLA) {
        snprintf(b, MGL_TAM_STRING, "%d", w->marcada ? 1 : 0);
        return b;
    }

    if (w->texto == NULL)
        return b;

    mgl_copiar_string(b, MGL_TAM_STRING, w->texto);

    return b;
}


static void mgl_fijar_texto_int(long long id, const char *texto)
{
    MglWidget *w = mgl_buscar((int) id);

    if (w == NULL || w->texto == NULL)
        return;

    if (w->kind == MGL_ENTRADA) {
        /* Respeta la capacidad del buffer de la entrada. */
        const char *src = texto != NULL ? texto : "";
        size_t max = (size_t) w->texto_max;

        snprintf(w->texto, max + 1, "%s", src);

        mgl_redibujar_si_ventana((int) id);

        return;
    }

    char *nuevo = mgl_duplicar(texto);

    if (nuevo == NULL)
        return;

    free(w->texto);

    w->texto = nuevo;

    /*
     * Las etiquetas y casillas redimensionan con su texto;
     * los botones conservan su caja.
     */
    if (w->kind == MGL_ETIQUETA) {
        int tw = 0;
        int th = 0;

        mgl_medir(w->texto, &tw, &th);

        w->ancho = tw > 0 ? tw : 1;
        w->alto = th > 0 ? th : 1;
    } else if (w->kind == MGL_CASILLA) {
        int tw = 0;

        mgl_medir(w->texto, &tw, NULL);

        w->ancho = MGL_CASILLA_LADO + 6 + (tw > 0 ? tw : 1);
    }

    mgl_redibujar_si_ventana((int) id);
}


static void mgl_fijar_marcada_int(long long id, long long marcada)
{
    MglWidget *w = mgl_buscar((int) id);

    if (w == NULL || w->kind != MGL_CASILLA)
        return;

    w->marcada = marcada != 0;

    mgl_redibujar_si_ventana((int) id);
}


static void mgl_habilitar_int(long long id, long long valor)
{
    MglWidget *w = mgl_buscar((int) id);

    if (w == NULL)
        return;

    w->habilitado = valor != 0;

    if (!w->habilitado && mgl.enfocado == w->id)
        mgl.enfocado = 0;

    mgl_redibujar_si_ventana((int) id);
}


void mgl_font(const char *nombre, long long tamano)
{
    if (!mgl_conectar())
        return;

    if (nombre == NULL || nombre[0] == '\0')
        nombre = "sans";

    if (tamano < 1) tamano = 12;
    if (tamano > 200) tamano = 200;

    snprintf(
        mgl.patrimonio_fuente,
        sizeof(mgl.patrimonio_fuente),
        "%s-%lld",
        nombre,
        tamano
    );

    if (!mgl_abrir_fuente())
        return;

    for (int i = 0; i < mgl.cantidad; i++) {
        if (mgl.widgets[i].kind == MGL_VENTANA)
            mgl_redibujar_ventana(&mgl.widgets[i]);
    }
}


static void mgl_color_texto_int(
    long long id,
    long long r,
    long long g,
    long long b
)
{
    MglWidget *w = mgl_buscar((int) id);

    if (w == NULL)
        return;

    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;

    w->fg = ((unsigned) r << 16) |
            ((unsigned) g << 8) |
            (unsigned) b;

    mgl_redibujar_si_ventana((int) id);
}


static void mgl_color_fondo_int(
    long long id,
    long long r,
    long long g,
    long long b
)
{
    MglWidget *w = mgl_buscar((int) id);

    if (w == NULL)
        return;

    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;

    w->bg = ((unsigned) r << 16) |
            ((unsigned) g << 8) |
            (unsigned) b;

    mgl_redibujar_si_ventana((int) id);
}


void mgl_title(long long ventana, const char *titulo)
{
    MglWidget *v = mgl_buscar_ventana((int) ventana);

    if (v == NULL)
        return;

    XStoreName(mgl.dpy, v->xwin, titulo != NULL ? titulo : "");
    XFlush(mgl.dpy);
}


void mgl_geometry(long long ventana, long long ancho, long long alto)
{
    MglWidget *v = mgl_buscar_ventana((int) ventana);

    if (v == NULL)
        return;

    if (ancho < 1) ancho = 1;
    if (alto < 1) alto = 1;

    v->ancho = (int) ancho;
    v->alto = (int) alto;

    XResizeWindow(mgl.dpy, v->xwin, (unsigned) ancho, (unsigned) alto);
    XFlush(mgl.dpy);

    mgl_redibujar_ventana(v);
}


void mgl_resizable(long long ventana, long long ancho, long long alto)
{
    MglWidget *v = mgl_buscar_ventana((int) ventana);

    if (v == NULL)
        return;

    XSizeHints hints;
    long suministrado = 0;

    memset(&hints, 0, sizeof(hints));
    XGetWMNormalHints(mgl.dpy, v->xwin, &hints, &suministrado);

    hints.flags |= PMinSize | PMaxSize;

    hints.min_width = ancho ? 1 : v->ancho;
    hints.max_width = ancho ? 100000 : v->ancho;

    hints.min_height = alto ? 1 : v->alto;
    hints.max_height = alto ? 100000 : v->alto;

    XSetWMNormalHints(mgl.dpy, v->xwin, &hints);
    XFlush(mgl.dpy);
}


long long mgl_config(long long id, const char *opcion, const char *valor)
{
    MglWidget *w = mgl_buscar((int) id);

    if (w == NULL || opcion == NULL)
        return 0;

    const char *v = valor != NULL ? valor : "";

    if (strcmp(opcion, "text") == 0) {
        if (w->texto == NULL)
            return 0;

        mgl_fijar_texto_int(id, v);
        return 1;
    }

    if (strcmp(opcion, "fg") == 0 || strcmp(opcion, "bg") == 0) {
        int r = 0;
        int g = 0;
        int b = 0;

        if (sscanf(v, "%d ,%d ,%d", &r, &g, &b) != 3)
            return 0;

        if (strcmp(opcion, "fg") == 0)
            mgl_color_texto_int(id, r, g, b);
        else
            mgl_color_fondo_int(id, r, g, b);

        return 1;
    }

    if (strcmp(opcion, "state") == 0) {
        if (strcmp(v, "disabled") == 0) {
            mgl_habilitar_int(id, 0);
            return 1;
        }

        if (strcmp(v, "normal") == 0 || strcmp(v, "enabled") == 0) {
            mgl_habilitar_int(id, 1);
            return 1;
        }

        return 0;
    }

    if (strcmp(opcion, "checked") == 0) {
        if (w->kind != MGL_CASILLA)
            return 0;

        if (strcmp(v, "true") == 0 || strcmp(v, "1") == 0) {
            mgl_fijar_marcada_int(id, 1);
            return 1;
        }

        if (strcmp(v, "false") == 0 || strcmp(v, "0") == 0) {
            mgl_fijar_marcada_int(id, 0);
            return 1;
        }

        return 0;
    }

    return 0;
}


void mgl_insert(long long id, const char *texto)
{
    MglWidget *w = mgl_buscar((int) id);

    if (w == NULL || w->texto == NULL || texto == NULL)
        return;

    if (w->kind == MGL_CASILLA || w->kind == MGL_MARCO)
        return;

    size_t actual = strlen(w->texto);
    size_t extra = strlen(texto);

    if (w->kind == MGL_ENTRADA) {
        /* Buffer de capacidad fija: nunca se realoca. */
        size_t max = (size_t) w->texto_max;

        if (actual >= max)
            return;

        if (actual + extra > max)
            extra = max - actual;

        memcpy(w->texto + actual, texto, extra);
        w->texto[actual + extra] = '\0';

        mgl_redibujar_si_ventana((int) id);
        return;
    }

    char *completo = malloc(actual + extra + 1);

    if (completo == NULL)
        return;

    memcpy(completo, w->texto, actual);
    memcpy(completo + actual, texto, extra);
    completo[actual + extra] = '\0';

    mgl_fijar_texto_int(id, completo);

    free(completo);
}


void mgl_delete(long long id, long long inicio, long long fin)
{
    MglWidget *w = mgl_buscar((int) id);

    if (w == NULL || w->texto == NULL)
        return;

    if (w->kind != MGL_ENTRADA)
        return;

    int largo = (int) strlen(w->texto);
    int a = (int) inicio;
    int b = (int) fin;

    if (a < 0) a = 0;
    if (a > largo) a = largo;
    if (b < 0) b = largo;
    if (b > largo) b = largo;
    if (b < a) return;

    memmove(w->texto + a, w->texto + b, (size_t) (largo - b) + 1);

    mgl_redibujar_si_ventana((int) id);
}


long long mgl_update(void)
{
    return mgl_bombar(0);
}


long long mgl_wait(void)
{
    return mgl_bombar(1);
}


long long mgl_event_type(void)
{
    return (long long) mgl.ev_tipo;
}


long long mgl_event_widget(void)
{
    return (long long) mgl.ev_widget;
}


const char *mgl_event_text(void)
{
    char *b = mgl_nuevo_buffer();

    mgl_copiar_string(b, MGL_TAM_STRING, mgl.ev_texto);

    return b;
}


long long mgl_event_key(void)
{
    return (long long) mgl.ev_tecla;
}


long long mgl_event_x(void)
{
    return (long long) mgl.ev_x;
}


long long mgl_event_y(void)
{
    return (long long) mgl.ev_y;
}
