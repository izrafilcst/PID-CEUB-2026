// sensor_bar.scad — Barra de sensores LFR (8× TCRT5000 / QRE1113)
// Fatiamento: PETG, 0.15mm camada, 50% infill, 4 perímetros
// Montagem: parafusos M2 × 6mm nos sensores, M3 no suporte vertical
// Rafael Costa — LFR ESP32

// ═══ PARÂMETROS ══════════════════════════════════════════════════════════════
SENSOR_COUNT    = 8;
SENSOR_SPACING  = 10;     // mm — centro a centro
SENSOR_W        = 5.5;    // mm — largura do corpo TCRT5000
SENSOR_H        = 5.5;    // mm — altura do corpo
SENSOR_T        = 1.8;    // mm — profundidade (plana no PCB)
SENSOR_DEPTH    = 4;      // mm — altura do solo ao sensor (spec)

BAR_THICK       = 3;      // mm — espessura da barra
BAR_HEIGHT      = 8;      // mm — altura total da barra
BAR_EXTRA       = 10;     // mm — extensão além dos sensores externos

MOUNT_H         = 40;     // mm — altura do suporte vertical até o chassi
MOUNT_W         = 12;     // mm — largura do suporte
MOUNT_SLOT_W    = 4;      // mm — slot de ajuste de altura (±5mm)

CABLE_CHANNEL_W = 8;      // mm — canal para flat cable/fios
CABLE_CHANNEL_H = 3;

TOL = 0.25;

// ═══ MÓDULOS ═════════════════════════════════════════════════════════════════

module sensorPocket() {
    // Encaixe para TCRT5000 — desliza pelo topo
    cube([SENSOR_W + TOL*2,
          SENSOR_T + TOL,
          SENSOR_H + 1]);
}

module m2Hole() {
    cylinder(d=2.4, h=20, $fn=20);
}

module m3Hole() {
    cylinder(d=3.4, h=30, $fn=20);
}

module slotHole(w, h, l) {
    // Slot oblongo para ajuste
    hull() {
        cylinder(d=w, h=l, $fn=20);
        translate([0, h, 0]) cylinder(d=w, h=l, $fn=20);
    }
}

// ═══ BARRA PRINCIPAL ════════════════════════════════════════════════════════

module sensorBar() {
    BAR_TOTAL = (SENSOR_COUNT - 1) * SENSOR_SPACING + SENSOR_W + BAR_EXTRA * 2;
    START_X   = BAR_EXTRA;

    difference() {
        // Corpo da barra
        translate([0, 0, 0])
            cube([BAR_TOTAL, BAR_THICK, BAR_HEIGHT]);

        // Pockets dos sensores (abertos pelo fundo para apontar ao solo)
        for (i = [0 : SENSOR_COUNT - 1]) {
            translate([START_X + i * SENSOR_SPACING - TOL,
                       BAR_THICK - SENSOR_T - TOL,
                       1])
                sensorPocket();
        }

        // Furos M2 de fixação dos sensores
        for (i = [0 : SENSOR_COUNT - 1]) {
            translate([START_X + i * SENSOR_SPACING + SENSOR_W/2,
                       -1,
                       BAR_HEIGHT / 2])
                rotate([-90, 0, 0])
                    m2Hole();
        }

        // Canal de cabos (topo da barra, centralizado)
        translate([(BAR_TOTAL - CABLE_CHANNEL_W) / 2,
                   -1,
                   BAR_HEIGHT - CABLE_CHANNEL_H - 1])
            cube([CABLE_CHANNEL_W, BAR_THICK + 2, CABLE_CHANNEL_H + 1]);
    }
}

// ═══ SUPORTE VERTICAL ═══════════════════════════════════════════════════════

module mountArm() {
    BAR_TOTAL = (SENSOR_COUNT - 1) * SENSOR_SPACING + SENSOR_W + BAR_EXTRA * 2;
    ARM_X     = BAR_TOTAL / 2 - MOUNT_W / 2;

    difference() {
        union() {
            // Haste vertical
            translate([ARM_X, 0, BAR_HEIGHT])
                cube([MOUNT_W, BAR_THICK, MOUNT_H]);

            // Reforço triangular (evita flex)
            translate([ARM_X, 0, BAR_HEIGHT])
                rotate([0, 0, 0])
                linear_extrude(height=BAR_THICK)
                    polygon([[0,0],
                             [MOUNT_W, 0],
                             [MOUNT_W, MOUNT_H * 0.4],
                             [0, MOUNT_H * 0.4]]);
        }

        // Slot de ajuste de altura (M3) no topo do suporte
        translate([ARM_X + MOUNT_W/2,
                   BAR_THICK/2,
                   BAR_HEIGHT + MOUNT_H - 5])
            rotate([90, 0, 0])
                slotHole(3.4, 10, BAR_THICK + 2);
    }
}

// ═══ RENDER ══════════════════════════════════════════════════════════════════
// Para exportar STL: openscad -o sensor_bar.stl sensor_bar.scad
sensorBar();
mountArm();
