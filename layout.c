// --- Library --------------------------------------------------------------------------
#include <string.h>
#include <assert.h>

struct ui_box {int x,y,w,h;};
typedef unsigned long long ui_id;
struct ui_node {
    int parent;
    int lst, end;
    int nxt, cnt;
    int siz[2];
};
struct ui_panel {
    ui_id id;
    struct ui_box box;
    int node;
};
enum ui_pass {
    // public
    UI_BLUEPRINT,
    UI_LAYOUT,
    UI_INPUT,
    UI_RENDER,

    // internal
    UI_INVALID,
    UI_FINISHED,
    UI_PASS_CNT
};
static int ui_pass = UI_BLUEPRINT;

// id stack
static int ui_id_stk_top;
#define UI_ID_STK_MAX  8
static ui_id ui_id_stk[UI_ID_STK_MAX];

// panel stack
static int ui_stk_top;
#define UI_STK_MAX 32
static int ui_stk[UI_STK_MAX];

// layout tree
#define UI_MAX_NODES (64*1024)
static struct ui_node ui_tree[UI_MAX_NODES];
static int ui_node_cnt = 0;

// table
#define UI_TBL_CNT (UI_MAX_NODES * 2)
#define UI_TBL_MSK (UI_TBL_CNT-1)
static ui_id ui_tbl_keys[UI_TBL_CNT];
static int ui_tbl_vals[UI_TBL_CNT];
static int ui_tbl_cnt = 0;

#define max(a,b) ((a) > (b) ? (a) : (b))

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
ui_add(ui_id key, int val)
{
    ui_id i = key & UI_TBL_MSK, b = i;
    do {if (ui_tbl_keys[i]) continue;
        ui_tbl_keys[i] = key;
        ui_tbl_vals[i] = val; return;
    } while ((i = ((i+1) & UI_TBL_MSK)) != b);
}
static int
ui_fnd(ui_id key)
{
    ui_id k, i = key & UI_TBL_MSK, b = i;
    do {if (!(k = ui_tbl_keys[i])) return 0;
        if (k == key) return (int)i;
    } while ((i = ((i+1) & UI_TBL_MSK)) != b);
    return UI_TBL_CNT;
}
static int
ui_add_node(ui_id id, int parent)
{
    assert(ui_node_cnt < UI_MAX_NODES);
    struct ui_node *n = &ui_tree[ui_node_cnt];
    n->parent = parent;
    if (parent >= 0) {
        struct ui_node *p = &ui_tree[parent];
        if (p->lst < 0) {
            p->end = ui_node_cnt;
            p->lst = p->end;
        } else {
            ui_tree[p->end].nxt = ui_node_cnt;
            p->end = ui_node_cnt;
        }
        p->cnt++;
    }
    n->nxt = n->lst = n->end = -1;
    ui_add(ui_hash(id), ui_node_cnt);
    return ui_node_cnt++;
}
static void
ui_panel_begin(struct ui_panel *pan, struct ui_box box)
{
    memset(pan,0,sizeof(*pan));
    pan->id = ui_gen_id();
    pan->box = box;

    switch (ui_pass) {
    case UI_BLUEPRINT: {
        assert(ui_stk_top < UI_STK_MAX);
        pan->node = ui_add_node(pan->id, ui_stk[ui_stk_top-1]);
        ui_stk[ui_stk_top++] = pan->node;
    } break;
    default: {
        const int idx = ui_fnd(ui_hash(pan->id));
        if (idx >= UI_TBL_CNT)
            ui_pass =  UI_INVALID;
        else pan->node = ui_tbl_vals[idx];
    } break; }
}
static void
ui_panel_end(struct ui_panel *pan)
{
    switch (ui_pass) {
    default: break;
    case UI_BLUEPRINT: {
        /* default blueprint */
        struct ui_node *n = ui_tree + pan->node;
        int i = n->lst;
        while (i != -1) {
            n->siz[0] = max(n->siz[0], ui_tree[i].siz[0]);
            n->siz[1] = max(n->siz[1], ui_tree[i].siz[1]);
            i = ui_tree[i].nxt;
        }
        assert(ui_stk_top > 0);
        ui_stk_top--;
    } break;}
}
static int
ui_begin(struct ui_panel* root, struct ui_box scr)
{
    switch (ui_pass) {
    default: break;
    case UI_BLUEPRINT: {
        ui_node_cnt = 0;
        memset(ui_tbl_keys, 0, sizeof(ui_tbl_keys));
        memset(ui_tbl_vals, 0, sizeof(ui_tbl_vals));
        ui_tbl_cnt = 0;
    } break;
    case UI_FINISHED: {
        ui_pass = UI_BLUEPRINT;
        return 0;
    }}
    ui_id_stk_top = 1;
    ui_id_stk[0] = 1;
    ui_stk_top = 1;
    ui_stk[0] = -1;
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
    case UI_INVALID: ui_pass = UI_BLUEPRINT; break;
    case UI_LAYOUT: ui_pass = UI_INPUT; break;
    case UI_INPUT: ui_pass = UI_RENDER; break;
    case UI_RENDER: ui_pass = UI_FINISHED; break;
    case UI_BLUEPRINT:  ui_pass = UI_LAYOUT; break;}
}

// --- Widgets --------------------------------------------------------------------------
enum ui_flow {
    UI_HORIZONTAL,
    UI_VERTICAL
};
struct ui_lay {
    struct ui_panel pan;
    enum ui_flow flow;
    int spacing;
    int at, node;
};
static void
ui_lay_begin(struct ui_lay *lay, enum ui_flow flow, struct ui_box box)
{
    lay->flow = flow;
    ui_panel_begin(&lay->pan, box);

    switch (ui_pass) {
    case UI_BLUEPRINT: break;
    default: {
        const struct ui_node *n = ui_tree + lay->pan.node;
        lay->pan.box.w = n->siz[0];
        lay->pan.box.h = n->siz[1];

        lay->at = lay->flow == UI_HORIZONTAL ? box.x: box.y;
        lay->node = ui_tree[lay->pan.node].lst;
        // TODO(micha): handle case box.w/h < node.siz
    } break;}
}
static struct ui_box
ui_lay_gen(struct ui_lay *lay)
{
    struct ui_box b = {0,0,0,0};
    assert(lay->node != -1);

    switch (ui_pass) {
    case UI_BLUEPRINT: break;
    default: {
        struct ui_node *n = ui_tree + lay->node;
        switch (lay->flow) {
        case UI_HORIZONTAL: {
            b = (struct ui_box){lay->at, lay->pan.box.y, n->siz[0], n->siz[1]};
            lay->at += n->siz[0] + lay->spacing;
        } break;
        case UI_VERTICAL: {
            b = (struct ui_box){lay->pan.box.x, lay->at, n->siz[0], n->siz[1]};
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
    case UI_BLUEPRINT: {
        struct ui_node *n = ui_tree + lay->pan.node;
        n->siz[0] = n->siz[1] = 0;

        int i = n->lst;
        while (i != -1) {
            switch (lay->flow) {
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
        case UI_BLUEPRINT: {
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
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    while( 1 ) {
        // run ui passes
        struct ui_panel root;
        struct ui_box scrn = (struct ui_box){0,0,1200,800};
        while (ui_begin(&root, scrn)) {
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
            ui_end(&root);
        }
    }
}