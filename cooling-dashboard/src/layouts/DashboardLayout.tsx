import {
  Activity,
  BellRing,
  Boxes,
  Gauge,
  Menu,
  RadioTower,
  Settings,
  Snowflake,
  X,
} from 'lucide-react'
import { useState } from 'react'
import {
  NavLink,
  Outlet,
} from 'react-router-dom'

import ApiStatus from '../components/ApiStatus'


const navigationItems = [
  {
    label: 'Overview',
    path: '/',
    icon: Gauge,
  },
  {
    label: 'Devices',
    path: '/devices',
    icon: Boxes,
  },
  {
    label: 'Telemetry',
    path: '/telemetry',
    icon: Activity,
  },
  {
    label: 'Active Alarms',
    path: '/alarms',
    icon: BellRing,
  },
  {
    label: 'Device Health',
    path: '/health',
    icon: RadioTower,
  },
  {
    label: 'Settings',
    path: '/settings',
    icon: Settings,
  },
]


interface SidebarContentProps {
  closeSidebar: () => void
}


function SidebarContent({
  closeSidebar,
}: SidebarContentProps) {
  return (
    <>
      <div className="flex h-20 items-center gap-3 border-b border-slate-800 px-6">
        <div className="flex size-11 items-center justify-center rounded-xl bg-cyan-500/15 text-cyan-400">
          <Snowflake size={26} />
        </div>

        <div>
          <h1 className="font-bold text-white">
            Cooling Cloud
          </h1>

          <p className="text-xs text-slate-500">
            IoT Monitoring Platform
          </p>
        </div>
      </div>

      <nav className="space-y-2 p-4">
        {navigationItems.map((item) => {
          const Icon = item.icon

          return (
            <NavLink
              key={item.path}
              to={item.path}
              end={item.path === '/'}
              onClick={closeSidebar}
              className={({ isActive }) =>
                [
                  'flex items-center gap-3 rounded-xl px-4 py-3 text-sm font-medium transition',
                  isActive
                    ? 'bg-cyan-500/15 text-cyan-300'
                    : 'text-slate-400 hover:bg-slate-800 hover:text-white',
                ].join(' ')
              }
            >
              <Icon size={19} />
              {item.label}
            </NavLink>
          )
        })}
      </nav>

      <div className="absolute bottom-0 left-0 right-0 border-t border-slate-800 p-5">
        <div className="rounded-xl bg-slate-800/70 p-4">
        <ApiStatus showRegion />
        </div>
      </div>
    </>
  )
}


function DashboardLayout() {
  const [
    sidebarOpen,
    setSidebarOpen,
  ] = useState(false)

  function closeSidebar() {
    setSidebarOpen(false)
  }

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100">
      {/* Desktop sidebar */}
      <aside className="fixed inset-y-0 left-0 z-30 hidden w-72 border-r border-slate-800 bg-slate-900/95 lg:block">
        <SidebarContent
          closeSidebar={closeSidebar}
        />
      </aside>

      {/* Mobile overlay */}
      {sidebarOpen && (
        <button
          type="button"
          aria-label="Close navigation"
          onClick={closeSidebar}
          className="fixed inset-0 z-40 bg-black/70 backdrop-blur-sm lg:hidden"
        />
      )}

      {/* Mobile sidebar */}
      <aside
        className={[
          'fixed inset-y-0 left-0 z-50 w-72 border-r border-slate-800 bg-slate-900 shadow-2xl transition-transform duration-300 lg:hidden',
          sidebarOpen
            ? 'translate-x-0'
            : '-translate-x-full',
        ].join(' ')}
      >
        <button
          type="button"
          aria-label="Close menu"
          onClick={closeSidebar}
          className="absolute right-4 top-5 flex size-10 items-center justify-center rounded-xl text-slate-400 hover:bg-slate-800 hover:text-white"
        >
          <X size={22} />
        </button>

        <SidebarContent
          closeSidebar={closeSidebar}
        />
      </aside>

      <div className="lg:pl-72">
        <header className="sticky top-0 z-20 flex h-20 items-center justify-between border-b border-slate-800 bg-slate-950/90 px-4 backdrop-blur md:px-8">
          <div className="flex items-center gap-3">
            <button
              type="button"
              aria-label="Open menu"
              onClick={() =>
                setSidebarOpen(true)
              }
              className="flex size-10 items-center justify-center rounded-xl border border-slate-800 bg-slate-900 text-slate-300 hover:border-cyan-500/40 hover:text-cyan-300 lg:hidden"
            >
              <Menu size={21} />
            </button>

            <div>
              <p className="hidden text-xs uppercase tracking-[0.2em] text-cyan-500 sm:block">
                Live monitoring
              </p>

              <h2 className="font-semibold text-white">
                <span className="hidden sm:inline">
                  Cooling Management System
                </span>

                <span className="sm:hidden">
                  Cooling Cloud
                </span>
              </h2>
            </div>
          </div>

          <div className="rounded-xl border border-slate-800 bg-slate-900 px-3 py-2 sm:px-4">
            <ApiStatus />
            </div>
        </header>

        <main className="p-4 sm:p-5 md:p-8">
          <Outlet />
        </main>
      </div>
    </div>
  )
}


export default DashboardLayout