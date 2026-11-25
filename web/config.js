// Conexion de la pagina con el servicio donde los plugins publican sus listas.
// Estos dos valores son publicos por diseno: solo permiten LEER los catalogos.
// Escribir exige la clave secreta que guarda cada plugin en el equipo del streamer.
window.ACSL_CONFIG = {
  url: 'https://hwwlbtwltxsgathbxhpl.supabase.co',
  key: '', // clave "publishable" (anon) del proyecto de Supabase
  // libreria para los avisos en tiempo real; si no carga, la pagina se actualiza por consulta
  realtime: 'https://cdn.jsdelivr.net/npm/@supabase/supabase-js@2.45.4/+esm'
};
