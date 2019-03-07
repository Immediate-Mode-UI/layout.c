```c
struct ui_box {
    int left, right;
    int top, bottom;
    int center_x, center_y;
    int width, height;
};
struct ui_mouse_input {
    int pos[2];
    int prev_pos[2];
    int drag_area[2];
    int scrl_delta[2];

    unsigned was_scrolled:1;
    unsigned left_pressed:1;
    unsigned left_released:1;
    unsigned left_clicked:1;
};
struct ui_input {
    struct ui_mouse_input mouse;
    unsigned key_mod;
    unsigned is_hovered:1;
    unsigned was_entered:1;
    unsigned was_left:1;
    unsigned has_focus:1;
    unsigned gained_focus:1;
    unsigned lost_focus:1;
};
typedef unsigned long long ui_id;
struct ui_node {
    int parent;
    int chd;
    int nxt;
    struct ui_box box;
};
struct ui_panel {
    ui_id id;
    struct ui_box box;
    struct ui_input input;
    struct ui_node *node;
};
enum ui_pass {
    UI_PASS_BLUEPRINT,
    UI_PASS_LAYOUT,
    UI_PASS_INPUT,
    UI_PASS_RENDER,
    UI_PASS_CNT
};
static int ui_pass = UI_PASS_RENDER;

// id stack
int ui_id_stk_top;
#define UI_ID_STK_MAX  8
ui_id ui_id_stk[UI_ID_STK_MAX];

// panel stack
int ui_stk_top;
#define UI_STK_MAX 32
int ui_stk[UI_PANEL_STK_MAX];

// layout tree
#define UI_MAX_NODES (64*1024)
static struct ui_node ui_tree[UI_MAX_PARAM];
static int ui_node_cnt = 0;

// table
static ui_id ui_tbl_keys[UI_MAX_NODES * 2];
static int ui_tbl_vals[UI_MAX_NODES * 2]
static int ui_tbl_cnt = 0;

static ui_id
ui_gen_id()
{
    ui_id id = ui_id_stk[ui_id_stk_top-1];
    ui_id_stk[ui_id_stk_top-1] = (id & ~ 0xffffffffllu) | ((id & 0xffffffffllu) + 1);
    return id;
}
static void
ui_add(ui_id key, int val)
{
    ui_id n = cast(ui_id, ui_tbl_cnt);
    ui_id i = key & (n-1), b = i;
    do {if (ui_tbl_keys[i]) continue;
        ui_tbl_keys[i] = key;
        ui_tbl_vals[i] = val; return;
    } while ((i = ((i+1) & (n-1))) != b);
}
static int
ui_fnd(ui_id key)
{
    ui_id k, n = cast(ui_id, ui_tbl_cnt);
    ui_id i = key & (n-1), b = i;
    do {if (!(k = ui_tbl_keys[i])) return 0;
        if (k == key) return i;
    } while ((i = ((i+1) & (n-1))) != b);
    return UI_MAX_NODES;
}
static struct ui_node*
ui_add_node(int parent)
{
    assert(ui_node_cnt < UI_MAX_NODES);
    struct ui_node *n = &ui_tree[ui_node_cnt++];    
    n->parent = parent;
    if (parent >= 0) {
        struct ui_node *p = &ui_tree[ui_node_cnt++];
        n->nxt = p->chd;
        p->chd = n - ui_tree;
    } 
    n->chd = -1;
    ui_add(pan->id, ui_node_cnt);
    return n;
}
static void
ui_panel_begin(struct ui_panel *pan, struct ui_box *box)
{
    memset(pan,0,sizeof(*pan));
    pan->id = ui_gen_id();
    pan->box = *box;

    switch (ui_pass) {
    default: break;
    case UI_PASS_BLUEPRINT: {
        assert(ui_stk_top < UI_STK_MAX);
        pan->node = ui_add_node(ui_stk[ui_stk_top]);
        ui_stk[ui_stk_top++] = pan->node - ui_tree;
    } break;}
}
static void
ui_panel_end(struct ui_panel *pan)
{
    switch (ui_pass) {
    default: break;
    case UI_PASS_BLUEPRINT: {
        assert(ui_stk_top > 0);
        ui_stk_top--;
    } break;}
}
static void
ui_begin(struct ui_panel* root, struct ui_box *scr)
{
    ui_id_stk[0] = -1;
    ui_stk_top = 1;
    ui_pass = (ui_pass + 1) % UI_PASS_CNT;
    
    switch (ui_pass) {
    case UI_PASS_BLUEPRINT: {
        /* reset layouting */
        ui_node_cnt = 0;
        memset(ui_tbl_keys, 0, sizeof(ui_tbl_keys));
        memset(ui_tbl_vals, 0, sizeof(ui_tbl_vals));
        ui_tbl_cnt = 0;
    } break;
    case UI_PASS_LAYOUT: {
        
    } break;
    case UI_PASS_INPUT: {
        
    } break;
    case UI_PASS_RENDER: {
        
    } break;}
    ui_panel_begin(root, &scr);
}
static void
ui_end()
{

}
```