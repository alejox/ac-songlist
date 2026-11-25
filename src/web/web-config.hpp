// Servicio donde los plugins publican su lista para la pagina web publica.
// La clave es la "publishable" del proyecto: es publica por diseno y por si sola solo deja
// leer. Publicar exige ademas la clave secreta que cada plugin genera y guarda en el equipo.
#pragma once

#define ACSL_WEB_API_URL "https://hwwlbtwltxsgathbxhpl.supabase.co"
#define ACSL_WEB_API_KEY "sb_publishable_45u3nKuHEWOeHF7eB6s2IA_V3hwMzyM"
#define ACSL_WEB_PAGE_URL "https://ac-songlist.vercel.app/"

// Ultima version publicada del plugin (para las actualizaciones automaticas).
#define ACSL_UPDATE_API_URL "https://api.github.com/repos/alejox/ac-songlist/releases/latest"
