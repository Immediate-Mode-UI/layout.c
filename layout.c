// --- Library --------------------------------------------------------------------------
#include <string.h>
#include <assert.h>

struct ui_box {int x,y,w,h;};
typedef unsigned long long ui_id;
struct ui_node {
    int parent;
    int lst, end, nxt;
    int siz[2];
};
struct ui_panel {
    ui_id id;
    struct ui_box box;
    int node;
    int max[2];
};
enum ui_pass {
    // public
    UI_LAYOUT,
    UI_INPUT,
    UI_RENDER,

    // internal
    UI_INVALID,
    UI_FINISHED,
    UI_PASS_CNT
};
static int ui_pass = UI_LAYOUT;

// id stack
#define UI_ID_STK_MAX  8
static int ui_id_stk_top;
static ui_id ui_id_stk[UI_ID_STK_MAX];

// panel stack
#define UI_STK_MAX 32
static int ui_stk_top;
static struct ui_panel* ui_stk[UI_STK_MAX];

// layout tree
#define UI_MAX_NODES (64*1024)
static struct ui_node ui_tree[UI_MAX_NODES];
static int ui_node_cnt = 0;

// table
#define UI_TBL_CNT (UI_MAX_NODES * 2)
#define UI_TBL_MSK (UI_TBL_CNT-1)
static ui_id ui_tbl_key[UI_TBL_CNT];
static int ui_tbl_val[UI_TBL_CNT];
static int ui_tbl_cnt = 0;

#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))
#define ui_box(x,y,w,h) (struct ui_box){x,y,w,h}

static ui_id
ui_gen_id()
{
    const ui_id id = ui_id_stk[ui_id_stk_top-1];
    ui_id_stk[ui_id_stk_top-1] = (id & ~0xffffffffllu) | ((id & 0xffffffffllu) + 1);
    return id;
}
static ui_id
ui_hash(ui_id x)
{
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9llu;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebllu;
    x ^= x >> 31;
    return x;
}
static void
ui_put(ui_id key, int val)
{
    ui_id i = key & UI_TBL_MSK, b = i;
    do {if (ui_tbl_key[i]) continue;
        ui_tbl_key[i] = key;
        ui_tbl_val[i] = val; return;
    } while ((i = ((i+1) & UI_TBL_MSK)) != b);
}
static int
ui_fnd(ui_id key)
{
    ui_id k, i = key & UI_TBL_MSK, b = i;
    do {if (!(k = ui_tbl_key[i])) return UI_TBL_CNT;
        if (k == key) return (int)i;
    } while ((i = ((i+1) & UI_TBL_MSK)) != b);
    return UI_TBL_CNT;
}
static int
ui_node(ui_id id, int parent)
{
    assert(ui_node_cnt < UI_MAX_NODES);
    struct ui_node *n = &ui_tree[ui_node_cnt];
    if ((n->parent = parent) >= 0) {
        struct ui_node *p = &ui_tree[parent];
        if (p->lst < 0)
            p->lst = ui_node_cnt;
        else ui_tree[p->end].nxt = ui_node_cnt;
        p->end = ui_node_cnt;
    }
    n->nxt = n->lst = n->end = -1;
    ui_put(ui_hash(id), ui_node_cnt);
    return ui_node_cnt++;
}
static void
ui_panel_begin(struct ui_panel *pan, struct ui_box box)
{
    memset(pan,0,sizeof(*pan));
    pan->id = ui_gen_id();
    pan->box = box;
    pan->node = -1;

    switch (ui_pass) {
    case UI_LAYOUT: {
        assert(ui_stk_top > 0);
        assert(ui_stk_top < UI_STK_MAX);
        struct ui_panel *p = ui_stk[ui_stk_top-1];
        pan->node = ui_node(pan->id, p ? p->node: -1);
        ui_stk[ui_stk_top++] = pan;
    } break;
    case UI_INPUT:
    case UI_RENDER: {
        const int idx = ui_fnd(ui_hash(pan->id));
        if (idx >= UI_TBL_CNT)
            ui_pass =  UI_INVALID;
        else pan->node = ui_tbl_val[idx];
    } break; }
}
static void
ui_panel_end(struct ui_panel *pan)
{
    struct ui_panel *p;
    assert(ui_stk_top > 0);
    if ((p = ui_stk[ui_stk_top-1])) {
        p->max[0] = pan->box.x + pan->box.w;
        p->max[1] = pan->box.y + pan->box.h;
    }
    switch (ui_pass) {
    default: break;
    case UI_LAYOUT: {
        /* default layouting */
        struct ui_node *n = ui_tree + pan->node;
        int i = n->lst;
        while (i != -1) {
            n->siz[0] = max(n->siz[0], ui_tree[i].siz[0]);
            n->siz[1] = max(n->siz[1], ui_tree[i].siz[1]);
            i = ui_tree[i].nxt;
        }
        ui_stk_top--;
    } break;}
}
static int
ui_begin(struct ui_panel* root, struct ui_box scr)
{
    switch (ui_pass) {
    default: break;
    case UI_LAYOUT: {
        ui_node_cnt = 0;
        memset(ui_tbl_key, 0, sizeof(ui_tbl_key));
        memset(ui_tbl_val, 0, sizeof(ui_tbl_val));
        ui_tbl_cnt = 0;
    } break;
    case UI_FINISHED: {
        ui_pass = UI_LAYOUT;
        return 0;
    }}
    ui_id_stk_top = 1;
    ui_id_stk[0] = 1;
    ui_stk_top = 1;
    ui_stk[0] = 0;
    ui_panel_begin(root, scr);
    return 1;
}
static void
ui_end(struct ui_panel* root)
{
    ui_panel_end(root);
    assert(ui_stk_top == 1);
    assert(ui_id_stk_top == 1);

    switch (ui_pass) {
    case UI_LAYOUT: ui_pass = UI_INPUT; break;
    case UI_INPUT: ui_pass = UI_RENDER; break;
    case UI_INVALID: ui_pass = UI_LAYOUT; break;
    case UI_RENDER: ui_pass = UI_FINISHED; break;}
}

// --- Widgets ------------------------------------------------------------------
enum ui_orient {
    UI_HORIZONTAL,
    UI_VERTICAL
};
struct ui_lay {
    struct ui_panel pan;
    enum ui_orient orient;
    int spacing;
    int at, node;
};
static void
ui_lay_begin(struct ui_lay *lay, enum ui_orient orient, struct ui_box box)
{
    lay->orient = orient;
    ui_panel_begin(&lay->pan, box);

    switch (ui_pass) {
    case UI_LAYOUT: break;
    case UI_INPUT:
    case UI_RENDER: {
        // TODO(micha): handle case box.w/h < node.siz
        const struct ui_node *n = ui_tree + lay->pan.node;
        lay->pan.box.w = n->siz[0];
        lay->pan.box.h = n->siz[1];

        lay->at = lay->orient == UI_HORIZONTAL ? box.x: box.y;
        lay->node = ui_tree[lay->pan.node].lst;
    } break;}
}
static struct ui_box
ui_lay_gen(struct ui_lay *lay)
{
    struct ui_box b = {0,0,0,0};
    assert(lay->node != -1);

    switch (ui_pass) {
    default: break;
    case UI_INPUT:
    case UI_RENDER: {
        struct ui_node *n = ui_tree + lay->node;
        switch (lay->orient) {
        case UI_HORIZONTAL: {
            b = ui_box(lay->at, lay->pan.box.y, n->siz[0], n->siz[1]);
            lay->at += n->siz[0] + lay->spacing;
        } break;
        case UI_VERTICAL: {
            b = ui_box(lay->pan.box.x, lay->at, n->siz[0], n->siz[1]);
            lay->at += n->siz[1] + lay->spacing;
        }}
        lay->node = ui_tree[lay->node].nxt;
        return b;
    }}
    return b;
}
static void
ui_lay_end(struct ui_lay *lay)
{
    ui_panel_end(&lay->pan);
    switch (ui_pass) {
    default: break;
    case UI_LAYOUT: {
        struct ui_node *n = ui_tree + lay->pan.node;
        n->siz[0] = n->siz[1] = 0;

        int i = n->lst;
        while (i != -1) {
            switch (lay->orient) {
            case UI_HORIZONTAL: {
                n->siz[0] += ui_tree[i].siz[0] + lay->spacing;
                n->siz[1] = max(n->siz[1], ui_tree[i].siz[1]);
            } break;
            case UI_VERTICAL: {
                n->siz[0] = max(n->siz[0], ui_tree[i].siz[0]);
                n->siz[1] += ui_tree[i].siz[1] + lay->spacing;
            } break;}
            i = ui_tree[i].nxt;
        }
    } break;}
}
static void
ui_lbl(struct ui_box box, const char *str_begin, const char *str_end)
{
    struct ui_panel pan;
    str_end = str_end ? str_end : str_begin + strlen(str_begin);
    ui_panel_begin(&pan, box);
    {
        switch (ui_pass) {
        default: break;
        case UI_LAYOUT: {
            /* some dummy code for calculating text size */
            #define TEST_CHAR_WIDTH 6
            #define TEST_CHAR_HEIGHT 12
            struct ui_node *n = ui_tree + pan.node;
            n->siz[0] = (int)((str_end - str_begin) * TEST_CHAR_WIDTH);
            n->siz[1] = TEST_CHAR_HEIGHT;
        } break;}
    }
    ui_panel_end(&pan);
}

// --- List ------------------------------------------------------------------
struct ui_lst_lay {
    int page_cnt;
    int at[2];
    int idx;
    struct ui_box box;
    int row_height;
};
struct ui_lst_view {
    float off;
    int off_idx;
    int num, begin, end;
    int total[2];
    int max[2];
    int at[2];
    int cnt;
};
static void
ui_lst_begin(struct ui_lst_lay *lst, struct ui_box box, int row_height)
{
    lst->box = box;
    lst->row_height = !row_height ? TEST_CHAR_HEIGHT: row_height;
    lst->page_cnt = box.h / lst->row_height;
    lst->at[0] = box.x;
    lst->at[1] = box.y;
    lst->idx = 0;
}
static struct ui_box
ui_lst_gen(struct ui_lst_lay *lst)
{
    struct ui_box b = ui_box(lst->at[0], lst->at[1], lst->box.w, lst->row_height);
    lst->at[1] += b.h;
    lst->idx++;
    return b;
}
static void
ui_lst_end(struct ui_lst_lay *lst)
{
    (void)lst;
}
static void
ui_lst_view(struct ui_lst_view *v, struct ui_lst_lay* ls, int num, float off)
{
    v->num = max(1, num) - 1;
    v->cnt = ls->page_cnt;

    v->off = off;
    v->off_idx = (int)(off / (float)ls->row_height);

    v->begin = v->off_idx;
    v->end = v->begin + ls->page_cnt + 1;
    v->end = min(v->end, v->num);

    v->at[1] = ls->at[1] + (int)(v->off_idx * ls->row_height);
    v->at[0] = ls->at[0];

    v->total[0] = ls->box.w;
    v->total[1] = v->num * ls->row_height;

    v->max[0] = ls->at[0] + v->total[0];
    v->max[1] = ls->at[1] + v->total[1];

    // apply view to layout
    ls->at[0] = v->at[0];
    ls->at[1] = v->at[1];
    ls->idx = v->begin;
}

// --- Test --------------------------------------------------------------------
int main(void)
{
    int running = 1;
    while (running) {
        struct ui_panel root;
        while (ui_begin(&root,ui_box(0,0,1200,800)))
        {
            // layouting
            struct ui_lay col = {.spacing = 8};
            ui_lay_begin(&col, UI_VERTICAL, root.box);
            {
                // first row
                struct ui_lay row0 = {.spacing = 4};
                ui_lay_begin(&row0, UI_HORIZONTAL, ui_lay_gen(&col));
                ui_lbl(ui_lay_gen(&row0), "Label",0);
                ui_lbl(ui_lay_gen(&row0), "Hallo World",0);
                ui_lbl(ui_lay_gen(&row0), "Hallo Universe",0);
                ui_lay_end(&row0);

                // second row
                struct ui_lay row1 = {.spacing = 4};
                ui_lay_begin(&row1, UI_HORIZONTAL, ui_lay_gen(&col));
                ui_lbl(ui_lay_gen(&row1), "Text",0);
                ui_lbl(ui_lay_gen(&row1), "Very long string to show of layouting",0);
                ui_lay_end(&row1);
            }
            ui_lay_end(&col);

            // list
            static float off = 50.0f;
            struct ui_panel area = {0};
            ui_panel_begin(&area, ui_box(50, 200 - (int)off, 600, 600));
            {
                struct ui_lst_lay lst_lay = {0};
                ui_lst_begin(&lst_lay, area.box, 0);
                {
                    struct ui_lst_view lst_view = {0};
                    ui_lst_view(&lst_view, &lst_lay, 1024, off);
                    for (int i = lst_view.begin; i < lst_view.end; ++i)
                        ui_lbl(ui_lst_gen(&lst_lay), "Item", 0);
                }
                ui_lst_end(&lst_lay);
            }
            ui_panel_end(&area);

            ui_end(&root);
        }
    }
    return 0;
}
