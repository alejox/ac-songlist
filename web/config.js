// Conexion de la pagina con el servicio donde los plugins publican sus listas.
// Estos dos valores son publicos por diseno: solo permiten LEER los catalogos.
// Escribir exige la clave secreta que guarda cada plugin en el equipo del streamer.
window.ACSL_CONFIG = {
  url: 'https://hwwlbtwltxsgathbxhpl.supabase.co',
  key: 'sb_publishable_45u3nKuHEWOeHF7eB6s2IA_V3hwMzyM', // clave "publishable" del proyecto de Supabase
  // libreria para los avisos en tiempo real (supabase-js 2.45.4, incluida en vendor/);
  // si no carga, la pagina se actualiza por consulta cada pocos segundos
  realtime: 'vendor/supabase.js'
};
