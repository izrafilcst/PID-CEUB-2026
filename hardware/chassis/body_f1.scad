// body_f1.scad — Chassis principal LFR formato F1 paramétrico
// Fatiamento: PETG, 0.2mm camada, 40% infill, 3 perímetros, suporte não necessário
// Dimensões máx competição: 250×250×200mm
// Rafael Costa — LFR ESP32

// ═══ PARÂMETROS ══════════════════════════════════════════════════════════════
// Corpo principal
BODY_LENGTH   = 180;   // mm — frente a trás
BODY_WIDTH    = 120;   // mm — largura total
BODY_HEIGHT   = 12;    // mm — espessura do piso
WALL          = 2.4;   // mm — espessura de parede (3 × 0.8mm bico)

// Recortes de componentes
ESP32_L       = 52;    // mm — ESP32 DevKit V1
ESP32_W       = 29;    // mm
ESP32_MARGIN  = 1.5;   // mm — folga por lado

LIPO_L        = 58;    // mm — LiPo 2S 800mAh
LIPO_W        = 32;
LIPO_H        = 12;

PCB_L         = 50;    // mm — perfboard shield
PCB_W         = 70;

// Motores N20
MOTOR_D       = 12;    // mm — diâmetro corpo motor
MOTOR_L       = 25;    // mm — comprimento
MOTOR_OFFSET  = 10;    // mm — recuo do eixo à traseira
MOTOR_SPACING = 80;    // mm — distância entre eixos L/R (center-to-center)

// Roda
WHEEL_D       = 34;    // mm — diâmetro silicone
WHEEL_W       = 8;     // mm — largura
AXLE_D        = 3;     // mm — diâmetro eixo

// Ball caster
CASTER_D      = 15;    // mm — esfera metálica
CASTER_POS_X  = BODY_LENGTH * 0.75;  // posição longitudinal (da frente)

// Sensor bar
SENSOR_BAR_L      = 90;   // mm — barra de sensores (8 × 10mm espaçamento)
SENSOR_BAR_OFFSET = 15;   // mm — avanço à frente do corpo
SENSOR_BAR_DROP   = 40;   // mm — profundidade do suporte vertical
SENSOR_HEIGHT     = 4;    // mm — altura do solo ao sensor

// Tolerância de impressão
TOL = 0.3;

// ═══ MÓDULOS ═════════════════════════════════════════════════════════════════

module roundedRect(l, w, h, r=3) {
    hull() {
        for (x = [r, l-r])
        for (y = [r, w-r])
            translate([x, y, 0]) cylinder(h=h, r=r, $fn=32);
    }
}

module motorCutout() {
    cylinder(d=MOTOR_D + TOL*2, h=MOTOR_L + 5, $fn=32);
}

module esp32Slot() {
    cube([ESP32_L + ESP32_MARGIN*2,
          ESP32_W + ESP32_MARGIN*2,
          BODY_HEIGHT + 1]);
}

module lipoBay() {
    cube([LIPO_L + TOL*2, LIPO_W + TOL*2, LIPO_H + TOL]);
}

module screwHole(d=3.2, h=20) {
    cylinder(d=d, h=h, $fn=24);
}

module screwBoss(d_outer=8, d_inner=3.2, h=6) {
    difference() {
        cylinder(d=d_outer, h=h, $fn=32);
        cylinder(d=d_inner, h=h+1, $fn=24);
    }
}

// ═══ CORPO PRINCIPAL ════════════════════════════════════════════════════════

module body() {
    difference() {
        // Piso principal — perfil F1: largo na traseira, estreito na frente
        hull() {
            // Traseira larga
            translate([0, 0, 0])
                roundedRect(BODY_WIDTH * 0.9, BODY_LENGTH * 0.45, BODY_HEIGHT, r=5);
            // Frente estreita
            translate([BODY_WIDTH * 0.1, BODY_LENGTH * 0.45, 0])
                roundedRect(BODY_WIDTH * 0.8, BODY_LENGTH * 0.55, BODY_HEIGHT, r=3);
        }

        // Recorte ESP32 (centro, traseira)
        translate([(BODY_WIDTH - ESP32_L - ESP32_MARGIN*2) / 2,
                   BODY_LENGTH * 0.05,
                   BODY_HEIGHT - 8])
            esp32Slot();

        // Recorte LiPo (centro, meio do chassi)
        translate([(BODY_WIDTH - LIPO_L - TOL*2) / 2,
                   BODY_LENGTH * 0.35,
                   BODY_HEIGHT - LIPO_H])
            lipoBay();

        // Furos dos motores N20 (sides)
        for (side = [-1, 1]) {
            translate([BODY_WIDTH/2 + side * MOTOR_SPACING/2,
                       BODY_LENGTH - MOTOR_OFFSET,
                       BODY_HEIGHT/2])
                rotate([0, 90, 0])
                    motorCutout();
        }

        // Furo ball caster (frente)
        translate([BODY_WIDTH/2, CASTER_POS_X, -1])
            cylinder(d=CASTER_D + TOL*2, h=BODY_HEIGHT+2, $fn=32);

        // Slot suporte barra de sensores
        translate([BODY_WIDTH/2 - 6, BODY_LENGTH - 3, 0])
            cube([12, WALL + 1, BODY_HEIGHT]);

        // Ventilação TB6612 / buck (furos 5mm em grid)
        for (x = [0:10:30], y = [0:10:30])
            translate([BODY_WIDTH*0.6 + x, BODY_LENGTH*0.5 + y, -1])
                cylinder(d=5, h=BODY_HEIGHT+2, $fn=16);
    }

    // Bosses M3 para montagem de PCB (4 cantos)
    for (x = [15, BODY_WIDTH - 15])
    for (y = [BODY_LENGTH*0.45, BODY_LENGTH*0.75])
        translate([x, y, BODY_HEIGHT])
            screwBoss(d_outer=7, d_inner=3.2, h=4);

    // Parede traseira (guarda bateria)
    translate([WALL, 0, 0])
        cube([BODY_WIDTH - WALL*2, WALL, BODY_HEIGHT + LIPO_H]);
}

// ═══ RENDER ════════════════════════════════════════════════════════════════
// Para exportar STL: openscad -o body_f1.stl body_f1.scad
body();
