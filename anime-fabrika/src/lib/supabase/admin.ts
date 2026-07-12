import { createClient as createSupabaseClient } from '@supabase/supabase-js'

// Сервисный клиент для воркера очереди (обходит RLS). Только на сервере.
export function createAdminClient() {
  return createSupabaseClient(
    process.env.NEXT_PUBLIC_SUPABASE_URL!,
    process.env.SUPABASE_SERVICE_ROLE_KEY!,
    { auth: { persistSession: false, autoRefreshToken: false } }
  )
}
