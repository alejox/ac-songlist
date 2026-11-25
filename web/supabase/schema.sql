-- AC Songlist: catalogo publico de canciones por canal.
--
-- Cada streamer publica su lista desde el plugin de OBS. La primera vez que un canal
-- publica, queda asociado a la clave secreta que genero ese plugin; a partir de ahi
-- solo quien tenga esa clave puede actualizarlo. Cualquiera puede leer.
--
-- Nadie escribe en las tablas directamente: todo pasa por publish_catalog().

create table if not exists public.catalogs (
  channel       text primary key check (channel ~ '^[a-z0-9_]{2,25}$'),
  title         text not null default '',
  songs         jsonb not null default '[]'::jsonb,   -- [{n,t,a,c:[...],p,l}]
  songs_version bigint not null default 1,
  updated_at    timestamptz not null default now(),
  created_at    timestamptz not null default now()
);

-- Lo que cambia a cada rato (sonando ahora y cola) va aparte, para no reenviar el
-- catalogo entero a todos los visitantes con cada pedido.
create table if not exists public.catalog_live (
  channel    text primary key references public.catalogs(channel) on delete cascade,
  live       jsonb not null default '{}'::jsonb,       -- {open, playing:{t,a,by:[...]}, queue:[{t,a,by:[...]}]}
  updated_at timestamptz not null default now()
);

-- Huella de la clave de cada canal. No se expone por la API.
create table if not exists public.catalog_keys (
  channel      text primary key references public.catalogs(channel) on delete cascade,
  key_hash     bytea not null,
  last_publish timestamptz not null default now()
);

alter table public.catalogs enable row level security;
alter table public.catalog_live enable row level security;
alter table public.catalog_keys enable row level security;

revoke all on public.catalogs, public.catalog_live, public.catalog_keys from anon, authenticated;
grant select on public.catalogs, public.catalog_live to anon, authenticated;

drop policy if exists "lectura publica" on public.catalogs;
create policy "lectura publica" on public.catalogs for select to anon, authenticated using (true);
drop policy if exists "lectura publica" on public.catalog_live;
create policy "lectura publica" on public.catalog_live for select to anon, authenticated using (true);
-- catalog_keys no tiene politicas: con RLS activo, nadie la lee ni la escribe por la API.

create or replace function public.publish_catalog(
  p_channel text,
  p_key     text,
  p_title   text  default null,
  p_songs   jsonb default null,   -- null = el catalogo no cambio, solo se actualiza "live"
  p_live    jsonb default null
) returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_hash     bytea;
  v_existing bytea;
  v_last     timestamptz;
begin
  if p_channel is null or p_channel !~ '^[a-z0-9_]{2,25}$' then
    raise exception 'canal no valido' using errcode = '22023';
  end if;
  if p_key is null or length(p_key) < 32 or length(p_key) > 128 then
    raise exception 'clave no valida' using errcode = '22023';
  end if;
  if p_songs is not null and (jsonb_typeof(p_songs) <> 'array' or jsonb_array_length(p_songs) > 3000
                              or octet_length(p_songs::text) > 700000) then
    raise exception 'catalogo demasiado grande' using errcode = '22023';
  end if;
  if p_live is not null and (jsonb_typeof(p_live) <> 'object' or octet_length(p_live::text) > 20000) then
    raise exception 'datos en vivo demasiado grandes' using errcode = '22023';
  end if;
  p_title := left(coalesce(p_title, ''), 80);
  v_hash := sha256(convert_to(p_key, 'UTF8'));

  select key_hash, last_publish into v_existing, v_last
    from public.catalog_keys where channel = p_channel for update;

  if not found then
    -- primer aviso de este canal: queda asociado a esta clave
    if (select count(*) from public.catalogs) >= 5000 then
      raise exception 'el servicio esta lleno' using errcode = '53400';
    end if;
    insert into public.catalogs (channel, title, songs) values (p_channel, p_title, coalesce(p_songs, '[]'::jsonb));
    insert into public.catalog_keys (channel, key_hash) values (p_channel, v_hash);
    insert into public.catalog_live (channel, live) values (p_channel, coalesce(p_live, '{}'::jsonb));
    return jsonb_build_object('status', 'created');
  end if;

  if v_existing <> v_hash then
    raise exception 'este canal ya lo publica otra instalacion' using errcode = '42501';
  end if;
  if v_last > now() - interval '700 milliseconds' then
    return jsonb_build_object('status', 'slow');   -- demasiado seguido: se ignora
  end if;
  update public.catalog_keys set last_publish = now() where channel = p_channel;

  if p_songs is not null then
    update public.catalogs
       set title = p_title, songs = p_songs, songs_version = songs_version + 1, updated_at = now()
     where channel = p_channel;
  end if;
  if p_live is not null then
    insert into public.catalog_live (channel, live) values (p_channel, p_live)
    on conflict (channel) do update set live = excluded.live, updated_at = now();
  end if;
  return jsonb_build_object('status', 'ok');
end;
$$;

-- Dejar de publicar: borra el catalogo del canal (solo con su clave).
create or replace function public.unpublish_catalog(p_channel text, p_key text) returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_existing bytea;
begin
  select key_hash into v_existing from public.catalog_keys where channel = p_channel;
  if not found then
    return jsonb_build_object('status', 'absent');
  end if;
  if p_key is null or v_existing <> sha256(convert_to(p_key, 'UTF8')) then
    raise exception 'este canal ya lo publica otra instalacion' using errcode = '42501';
  end if;
  delete from public.catalogs where channel = p_channel;
  return jsonb_build_object('status', 'deleted');
end;
$$;

revoke all on function public.publish_catalog(text, text, text, jsonb, jsonb) from public;
revoke all on function public.unpublish_catalog(text, text) from public;
grant execute on function public.publish_catalog(text, text, text, jsonb, jsonb) to anon, authenticated;
grant execute on function public.unpublish_catalog(text, text) to anon, authenticated;

-- Avisos en tiempo real a la pagina web.
do $$
begin
  if exists (select 1 from pg_publication where pubname = 'supabase_realtime') then
    if not exists (select 1 from pg_publication_tables where pubname = 'supabase_realtime' and tablename = 'catalogs') then
      alter publication supabase_realtime add table public.catalogs;
    end if;
    if not exists (select 1 from pg_publication_tables where pubname = 'supabase_realtime' and tablename = 'catalog_live') then
      alter publication supabase_realtime add table public.catalog_live;
    end if;
  end if;
end $$;
