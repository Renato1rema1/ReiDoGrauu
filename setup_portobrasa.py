# -*- coding: utf-8 -*-
"""
Rei do Grau — monta o projeto dentro do Unreal Editor.

Como usar: no editor, menu Tools > Execute Python Script... e escolha este arquivo.
Ele cria (ou recria):
  1. os materiais-mestre em /Game/ReiDoGrau/Materials (asfalto molhado com poças, calçadão de ondas,
     fachadas com janelas acesas, pintura com verniz, mar e brilho dos marcadores);
  2. o mapa /Game/ReiDoGrau/Maps/L_PortoBrasa com o pôr do sol, a cidade, o trânsito e o ponto de partida.
Pode rodar de novo quando quiser: ele refaz os materiais e os atores do mapa.
"""
import unreal

MAT_DIR = "/Game/ReiDoGrau/Materials"
MAP_PATH = "/Game/ReiDoGrau/Maps/L_PortoBrasa"

AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def log(msg):
    unreal.log("[Rei do Grau] " + msg)


# ----------------------------------------------------------------- utilidades de material

def make_material(name):
    path = f"{MAT_DIR}/{name}"
    if EAL.does_asset_exist(path):
        mat = EAL.load_asset(path)
        MEL.delete_all_material_expressions(mat)
    else:
        mat = AT.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    return mat


def node(mat, cls, x, y):
    return MEL.create_material_expression(mat, cls, x, y)


def vparam(mat, name, value, x, y):
    n = node(mat, unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(*value))
    return n


def sparam(mat, name, value, x, y):
    n = node(mat, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", value)
    return n


def const(mat, value, x, y):
    n = node(mat, unreal.MaterialExpressionConstant, x, y)
    n.set_editor_property("r", value)
    return n


def custom(mat, desc, code, out_type, input_names, x, y):
    n = node(mat, unreal.MaterialExpressionCustom, x, y)
    n.set_editor_property("description", desc)
    n.set_editor_property("code", code)
    n.set_editor_property("output_type", out_type)
    inputs = []
    for name in input_names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    n.set_editor_property("inputs", inputs)
    return n


def link(a, b, input_name, out_name=""):
    if not MEL.connect_material_expressions(a, out_name, b, input_name):
        log(f"aviso: não liguei a entrada {input_name}")


def to_prop(a, prop, out_name=""):
    MEL.connect_material_property(a, out_name, prop)


def finish(mat, name):
    MEL.recompile_material(mat)
    EAL.save_asset(f"{MAT_DIR}/{name}")
    log(f"material {name} pronto")


F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3

# ruído de valor em duas oitavas (repetido dentro de cada nó Custom)
NOISE = """
float2 q = WP.xy / 900.0;
float n = 0.0;
float amp = 0.65;
for (int o = 0; o < 2; o++) {
    float2 i = floor(q); float2 f = frac(q); f = f * f * (3.0 - 2.0 * f);
    float a = frac(sin(dot(i, float2(127.1, 311.7))) * 43758.5453);
    float b = frac(sin(dot(i + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
    float c = frac(sin(dot(i + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
    float d = frac(sin(dot(i + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
    n += amp * lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
    q *= 2.7; amp *= 0.35;
}
float puddle = smoothstep(0.52, 0.66, n) * saturate(Wet * 1.2);
"""


def build_surface():
    """Superfície PBR genérica: asfalto molhado, calçada, plástico, metal, emissivo."""
    name = "M_Grau_Surface"
    m = make_material(name)
    wp = node(m, unreal.MaterialExpressionWorldPosition, -1200, -200)
    base = vparam(m, "BaseColor", (0.5, 0.5, 0.5, 1.0), -1200, 0)
    rough = sparam(m, "Roughness", 0.5, -1200, 200)
    metal = sparam(m, "Metallic", 0.0, -1200, 300)
    wet = sparam(m, "Wetness", 0.0, -1200, 400)
    pattern = sparam(m, "Pattern", 0.0, -1200, 500)
    emis = vparam(m, "EmissiveColor", (0.0, 0.0, 0.0, 1.0), -1200, 600)
    emis_k = sparam(m, "EmissiveStrength", 0.0, -1200, 800)

    color_code = NOISE + """
float3 col = Base;
if (Pattern > 0.5) {
    // pedra portuguesa em ondas (calçadão de Copacabana)
    float2 p = WP.xy / 400.0;
    float band = frac(p.y * 4.0 + sin(p.x * 6.2831 * 1.5) * 0.32);
    col = lerp(float3(0.6, 0.58, 0.54), float3(0.03, 0.03, 0.03), step(0.5, band));
    float2 g = frac(WP.xy / 7.0);
    col *= 0.85 + 0.15 * step(0.12, min(g.x, g.y));
}
// molhado escurece a superfície
col *= lerp(1.0, 0.55, saturate(puddle + Wet * 0.3));
return col;
"""
    rough_code = NOISE + """
return lerp(Rough, 0.02, saturate(puddle + Wet * 0.55));
"""
    c_col = custom(m, "Cor + calçadão + molhado", color_code, F3, ["WP", "Base", "Pattern", "Wet"], -700, -100)
    link(wp, c_col, "WP"); link(base, c_col, "Base"); link(pattern, c_col, "Pattern"); link(wet, c_col, "Wet")
    c_rough = custom(m, "Poças (rugosidade)", rough_code, F1, ["WP", "Rough", "Wet"], -700, 200)
    link(wp, c_rough, "WP"); link(rough, c_rough, "Rough"); link(wet, c_rough, "Wet")
    mul = node(m, unreal.MaterialExpressionMultiply, -700, 650)
    link(emis, mul, "A"); link(emis_k, mul, "B")
    to_prop(c_col, unreal.MaterialProperty.MP_BASE_COLOR)
    to_prop(c_rough, unreal.MaterialProperty.MP_ROUGHNESS)
    to_prop(metal, unreal.MaterialProperty.MP_METALLIC)
    to_prop(mul, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m, name)


BUILDING_SETUP = """
float style = floor(T3);
float seed = frac(T3);
float3 tint = float3(T0, T1, T2);
float3 an = abs(N);
float u = an.x > an.y ? WP.y : WP.x;
float v = WP.z;
float2 cell = float2(u / 300.0, v / 320.0);
float2 id = floor(cell);
float2 f = frac(cell);
float win;
if (style < 0.5) win = step(0.04, f.x) * step(f.x, 0.96) * step(0.18, f.y) * step(f.y, 0.98);
else if (style < 1.5) win = step(0.19, f.x) * step(f.x, 0.81) * step(0.25, f.y) * step(f.y, 0.78);
else win = step(0.27, f.x) * step(f.x, 0.73) * step(0.28, f.y) * step(f.y, 0.74);
float roof = step(0.7, an.z);
float ground = step(v, 400.0);
win *= (1.0 - roof) * (1.0 - ground);
"""


def build_building():
    """Fachadas com janelas procedurais. Cor e estilo vêm de cada instância (Per Instance Custom Data)."""
    name = "M_Grau_Building"
    m = make_material(name)
    wp = node(m, unreal.MaterialExpressionWorldPosition, -1300, -300)
    nrm = node(m, unreal.MaterialExpressionVertexNormalWS, -1300, -150)
    data = []
    for i in range(4):
        d = node(m, unreal.MaterialExpressionPerInstanceCustomData, -1300, 0 + i * 120)
        d.set_editor_property("data_index", i)
        d.set_editor_property("default_value", [0.8, 0.8, 0.8, 1.2][i])
        data.append(d)
    names = ["WP", "N", "T0", "T1", "T2", "T3"]

    def hooked(desc, code, out_type, x, y):
        c = custom(m, desc, code, out_type, names, x, y)
        link(wp, c, "WP"); link(nrm, c, "N")
        for i in range(4):
            link(data[i], c, f"T{i}")
        return c

    col = hooked("Fachada (cor)", BUILDING_SETUP + """
float3 wall = tint * (style < 0.5 ? 0.12 : 0.62);
float3 glass = style < 0.5 ? float3(0.16, 0.2, 0.25) : float3(0.07, 0.09, 0.11);
float3 c = lerp(wall, glass, win);
c = lerp(c, float3(0.07, 0.065, 0.06), ground * (1.0 - roof));
return lerp(c, tint * 0.3, roof);
""", F3, -700, -200)
    rough = hooked("Fachada (rugosidade)", BUILDING_SETUP + """
return lerp(style < 0.5 ? 0.45 : 0.85, 0.05, win);
""", F1, -700, 50)
    metal = hooked("Fachada (metal)", BUILDING_SETUP + """
return win * (style < 0.5 ? 0.9 : 0.5);
""", F1, -700, 250)
    emis = hooked("Janelas e lojas acesas", BUILDING_SETUP + """
float h = frac(sin(dot(id + seed * 57.0, float2(12.9898, 78.233))) * 43758.5453);
float lit = style < 0.5 ? 0.22 : (style < 1.5 ? 0.36 : 0.24);
float3 warm = lerp(float3(1.0, 0.6, 0.28), float3(1.0, 0.86, 0.66), frac(h * 7.0));
float3 e = win * step(h, lit) * warm * 7.0 * (0.4 + 0.6 * f.y);
// térreo com vitrines acesas
float sf = frac(u / 600.0);
float sid = floor(u / 600.0);
float on = step(0.25, frac(sin(sid * 91.7 + seed * 13.0) * 4375.5));
float shop = step(0.06, sf) * step(sf, 0.94) * step(40.0, v) * step(v, 300.0) * (1.0 - roof);
e += shop * on * lerp(float3(1.0, 0.72, 0.42), float3(0.85, 0.92, 1.0), frac(sid * 0.37)) * 9.0;
return e;
""", F3, -700, 450)
    to_prop(col, unreal.MaterialProperty.MP_BASE_COLOR)
    to_prop(rough, unreal.MaterialProperty.MP_ROUGHNESS)
    to_prop(metal, unreal.MaterialProperty.MP_METALLIC)
    to_prop(emis, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m, name)


def build_paint():
    """Pintura automotiva com verniz (clear coat)."""
    name = "M_Grau_CarPaint"
    m = make_material(name)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    color = vparam(m, "PaintColor", (0.02, 0.02, 0.025, 1.0), -600, 0)
    metal = sparam(m, "Metallic", 0.55, -600, 150)
    rough = sparam(m, "Roughness", 0.3, -600, 250)
    coat = const(m, 1.0, -600, 350)
    coat_r = sparam(m, "ClearCoatRoughness", 0.05, -600, 450)
    to_prop(color, unreal.MaterialProperty.MP_BASE_COLOR)
    to_prop(metal, unreal.MaterialProperty.MP_METALLIC)
    to_prop(rough, unreal.MaterialProperty.MP_ROUGHNESS)
    to_prop(coat, unreal.MaterialProperty.MP_CUSTOM_DATA0)
    to_prop(coat_r, unreal.MaterialProperty.MP_CUSTOM_DATA1)
    finish(m, name)


def build_water():
    """Mar escuro e espelhado com ondas animadas (normal em espaço de mundo)."""
    name = "M_Grau_Water"
    m = make_material(name)
    m.set_editor_property("tangent_space_normal", False)
    wp = node(m, unreal.MaterialExpressionWorldPosition, -900, 0)
    t = node(m, unreal.MaterialExpressionTime, -900, 150)
    nrm = custom(m, "Ondas", """
float2 p = WP.xy / 100.0;
float2 g = float2(0, 0);
float2 dirs[5] = { float2(0.3, 1.0), float2(-0.6, 0.8), float2(1.0, 0.2), float2(0.7, -0.7), float2(-0.2, 1.0) };
float ks[5] = { 0.35, 0.6, 1.1, 1.9, 3.1 };
float amps[5] = { 0.05, 0.035, 0.02, 0.012, 0.007 };
for (int i = 0; i < 5; i++) {
    float2 d = normalize(dirs[i]);
    float ph = dot(d, p) * ks[i] + T * (0.8 + ks[i] * 0.6);
    g += amps[i] * ks[i] * cos(ph) * d;
}
return normalize(float3(-g.x * 4.0, -g.y * 4.0, 1.0));
""", F3, ["WP", "T"], -500, 0)
    link(wp, nrm, "WP"); link(t, nrm, "T")
    color = vparam(m, "WaterColor", (0.008, 0.025, 0.035, 1.0), -500, -200)
    to_prop(color, unreal.MaterialProperty.MP_BASE_COLOR)
    to_prop(const(m, 0.03, -500, 250), unreal.MaterialProperty.MP_ROUGHNESS)
    to_prop(const(m, 0.0, -500, 330), unreal.MaterialProperty.MP_METALLIC)
    to_prop(nrm, unreal.MaterialProperty.MP_NORMAL)
    finish(m, name)


def build_glow():
    """Coluna de luz dos desafios: aditiva, sem iluminação, some no alto."""
    name = "M_Grau_Glow"
    m = make_material(name)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    wp = node(m, unreal.MaterialExpressionWorldPosition, -900, 0)
    op = node(m, unreal.MaterialExpressionObjectPositionWS, -900, 150)
    color = vparam(m, "GlowColor", (1.0, 0.42, 0.1, 1.0), -900, 300)
    k = sparam(m, "GlowStrength", 6.0, -900, 450)
    c = custom(m, "Brilho com fade", """
float h = saturate((WP.z - OP.z) / 4500.0 + 0.5);
return Color * Strength * pow(1.0 - h, 2.0);
""", F3, ["WP", "OP", "Color", "Strength"], -500, 100)
    link(wp, c, "WP"); link(op, c, "OP"); link(color, c, "Color"); link(k, c, "Strength")
    to_prop(c, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m, name)


# ----------------------------------------------------------------- mapa

def build_map():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if EAL.does_asset_exist(MAP_PATH):
        les.load_level(MAP_PATH)
    else:
        les.new_level(MAP_PATH)

    remove = {"GrauWorldSetup", "GrauCityGenerator", "GrauTrafficManager", "PlayerStart"}
    for actor in eas.get_all_level_actors():
        if actor.get_class().get_name() in remove:
            eas.destroy_actor(actor)

    def spawn(class_path, loc, yaw=0.0, label=None):
        cls = unreal.load_class(None, class_path)
        if cls is None:
            log(f"ERRO: classe {class_path} não encontrada. Compile o projeto C++ primeiro.")
            return None
        actor = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        if actor and label:
            actor.set_actor_label(label)
        return actor

    spawn("/Script/ReiDoGrau.GrauWorldSetup", (0, 0, 0), label="Por do Sol")
    spawn("/Script/ReiDoGrau.GrauCityGenerator", (0, 0, 0), label="Porto Brasa")
    spawn("/Script/ReiDoGrau.GrauTrafficManager", (0, 0, 0), label="Transito")
    start = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(10000, 660, 150), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    if start:
        start.set_actor_label("Partida na Orla")
    les.save_current_level()
    log("mapa L_PortoBrasa salvo")


def main():
    with unreal.ScopedSlowTask(7, "Montando Porto Brasa...") as task:
        task.make_dialog(True)
        for step in (build_surface, build_building, build_paint, build_water, build_glow):
            task.enter_progress_frame(1, step.__doc__.split(".")[0] if step.__doc__ else step.__name__)
            try:
                step()
            except Exception as e:  # um material com problema não impede o resto
                log(f"falha em {step.__name__}: {e}")
        task.enter_progress_frame(1, "Criando o mapa")
        build_map()
        task.enter_progress_frame(1, "Salvando")
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("Pronto! Aperte Play (Alt+P).")


main()
