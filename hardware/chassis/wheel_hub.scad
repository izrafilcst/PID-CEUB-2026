// wheel_hub.scad — Hub de roda para O-ring silicone 34mm OD
// Fatiamento: PETG, 0.15mm camada, 60% infill, 4 perímetros, sem suporte
// Eixo N20: 3mm × D achatado — usar flat do motor para anti-giro
// Rafael Costa — LFR ESP32

// ═══ PARÂMETROS ══════════════════════════════════════════════════════════════
// Roda
WHEEL_OD      = 34;    // mm — diâmetro externo (O-ring silicone)
WHEEL_WIDTH   = 8;     // mm — largura total do hub
ORING_GROOVE_D = 30;   // mm — diâmetro do fundo do canal do O-ring
ORING_GROOVE_W = 3.5;  // mm — largura do canal (O-ring 3mm seção)
ORING_GROOVE_D_ORING = 3; // mm — seção do O-ring

// Eixo N20 (3mm, achatado em D)
AXLE_D        = 3.0;   // mm — diâmetro total
AXLE_FLAT     = 2.5;   // mm — distância flat→ centro (corte D)
AXLE_DEPTH    = 10;    // mm — profundidade do furo de encaixe

// Hub
HUB_D         = 14;    // mm — diâmetro do cubo central
HUB_SPOKE_W   = 3;     // mm — largura dos raios
SPOKE_COUNT   = 5;     // raios

// Fixação extra
GRUB_D        = 3.0;   // mm — furo para parafuso M3 grub (anti-rotação)
GRUB_DEPTH    = 8;     // mm

TOL = 0.2;

// ═══ MÓDULOS ═════════════════════════════════════════════════════════════════

module dShaft(d, flat, depth) {
    // Furo em D para eixo N20
    difference() {
        cylinder(d=d + TOL*2, h=depth + 1, $fn=32);
        // Remove excesso para criar flat
        translate([-d, flat - d/2, -1])
            cube([d*2, d, depth + 3]);
    }
}

module oringGroove() {
    // Canal toroidal para O-ring silicone
    rotate_extrude($fn=128)
        translate([ORING_GROOVE_D/2, WHEEL_WIDTH/2, 0])
            circle(d=ORING_GROOVE_D_ORING + TOL, $fn=32);
}

module spoke() {
    // Raio trapezoidal (mais largo na borda)
    linear_extrude(height=WHEEL_WIDTH)
        polygon([
            [-HUB_SPOKE_W/2, HUB_D/2],
            [ HUB_SPOKE_W/2, HUB_D/2],
            [ HUB_SPOKE_W*1.5, ORING_GROOVE_D/2 - 2],
            [-HUB_SPOKE_W*1.5, ORING_GROOVE_D/2 - 2]
        ]);
}

// ═══ ASSEMBLY ════════════════════════════════════════════════════════════════

module wheelHub() {
    difference() {
        union() {
            // Aro externo
            difference() {
                cylinder(d=WHEEL_OD, h=WHEEL_WIDTH, $fn=128);
                translate([0, 0, -1])
                    cylinder(d=ORING_GROOVE_D - ORING_GROOVE_D_ORING, h=WHEEL_WIDTH+2, $fn=128);
            }

            // Cubo central
            cylinder(d=HUB_D, h=WHEEL_WIDTH, $fn=64);

            // Raios
            for (i = [0 : SPOKE_COUNT - 1])
                rotate([0, 0, i * 360 / SPOKE_COUNT])
                    spoke();
        }

        // Canal O-ring
        oringGroove();

        // Furo do eixo em D (passante, centro)
        translate([0, 0, -1])
            dShaft(AXLE_D, AXLE_FLAT, AXLE_DEPTH + 1);

        // Furo grub screw M3 (perpendicular ao eixo, no cubo)
        translate([HUB_D/2 + 1, 0, AXLE_DEPTH/2])
            rotate([0, -90, 0])
                cylinder(d=GRUB_D, h=GRUB_DEPTH, $fn=20);
    }
}

// ═══ RENDER ══════════════════════════════════════════════════════════════════
// Para exportar STL: openscad -o wheel_hub.stl wheel_hub.scad
// Imprimir 2× (roda L + roda R — simétricas)
wheelHub();
