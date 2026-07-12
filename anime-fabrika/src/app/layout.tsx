import type { Metadata } from 'next'
import './globals.css'

export const metadata: Metadata = {
  title: 'Аниме-фабрика',
  description: 'Внутренняя платформа серийного производства аниме-контента',
}

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="ru" className="dark">
      <body className="min-h-screen antialiased">{children}</body>
    </html>
  )
}
