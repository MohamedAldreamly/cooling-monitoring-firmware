import { useQuery } from '@tanstack/react-query'
import type { LucideIcon } from 'lucide-react'
import {
  AlertTriangle,
  BellRing,
  Boxes,
  CircleCheck,
  CircleX,
  RefreshCw,
} from 'lucide-react'

import {
  getActiveAlarms,
  getDashboardSummary,
  getDevices,
} from '../api/dashboardApi'


interface SummaryCardProps {
  title: string
  value: number
  icon: LucideIcon
  color: 'cyan' | 'green' | 'red' | 'amber'
}


const colorStyles = {
  cyan: {
    container: 'border-cyan-500/20 bg-cyan-500/5',
    icon: 'bg-cyan-500/15 text-cyan-400',
  },
  green: {
    container: 'border-emerald-500/20 bg-emerald-500/5',
    icon: 'bg-emerald-500/15 text-emerald-400',
  },
  red: {
    container: 'border-red-500/20 bg-red-500/5',
    icon: 'bg-red-500/15 text-red-400',
  },
  amber: {
    container: 'border-amber-500/20 bg-amber-500/5',
    icon: 'bg-amber-500/15 text-amber-400',
  },
}


function SummaryCard({
  title,
  value,
  icon: Icon,
  color,
}: SummaryCardProps) {
  const styles = colorStyles[color]

  return (
    <article
      className={[
        'rounded-2xl border p-5',
        styles.container,
      ].join(' ')}
    >
      <div className="flex items-start justify-between">
        <div>
          <p className="text-sm text-slate-400">
            {title}
          </p>

          <p className="mt-3 text-3xl font-bold text-white">
            {value}
          </p>
        </div>

        <div
          className={[
            'flex size-11 items-center justify-center rounded-xl',
            styles.icon,
          ].join(' ')}
        >
          <Icon size={22} />
        </div>
      </div>
    </article>
  )
}


function severityClass(
  severity?: string,
) {
  switch (severity?.toUpperCase()) {
    case 'CRITICAL':
      return 'bg-red-500/15 text-red-400'

    case 'HIGH':
      return 'bg-orange-500/15 text-orange-400'

    case 'WARNING':
      return 'bg-amber-500/15 text-amber-400'

    default:
      return 'bg-slate-700 text-slate-300'
  }
}


function DashboardPage() {
  const summaryQuery = useQuery({
    queryKey: ['dashboard-summary'],
    queryFn: getDashboardSummary,
    refetchInterval: 10000,
  })

  const devicesQuery = useQuery({
    queryKey: ['devices'],
    queryFn: getDevices,
    refetchInterval: 15000,
  })

  const alarmsQuery = useQuery({
    queryKey: ['active-alarms'],
    queryFn: getActiveAlarms,
    refetchInterval: 10000,
  })

  const isLoading =
    summaryQuery.isLoading
    || devicesQuery.isLoading
    || alarmsQuery.isLoading

  const hasError =
    summaryQuery.isError
    || devicesQuery.isError
    || alarmsQuery.isError

  const isRefreshing =
    summaryQuery.isFetching
    || devicesQuery.isFetching
    || alarmsQuery.isFetching

  const summary = summaryQuery.data
  const devices =
    devicesQuery.data?.devices ?? []
  const activeAlarms =
    alarmsQuery.data?.alarms ?? []

  function refreshDashboard() {
    void Promise.all([
      summaryQuery.refetch(),
      devicesQuery.refetch(),
      alarmsQuery.refetch(),
    ])
  }

  if (isLoading) {
    return (
      <div className="flex min-h-80 items-center justify-center">
        <div className="text-center">
          <RefreshCw
            className="mx-auto animate-spin text-cyan-400"
            size={32}
          />

          <p className="mt-4 text-slate-400">
            Loading dashboard data...
          </p>
        </div>
      </div>
    )
  }

  if (hasError || !summary) {
    return (
      <div className="rounded-2xl border border-red-500/20 bg-red-500/5 p-8">
        <h2 className="text-xl font-semibold text-red-400">
          Unable to load dashboard
        </h2>

        <p className="mt-2 text-slate-400">
          Check the API URL, CORS configuration and internet connection.
        </p>

        <button
          type="button"
          onClick={refreshDashboard}
          className="mt-5 rounded-xl bg-red-500 px-5 py-2.5 font-medium text-white hover:bg-red-400"
        >
          Try again
        </button>
      </div>
    )
  }

  return (
    <section>
      <div className="flex flex-col justify-between gap-4 md:flex-row md:items-center">
        <div>
          <h1 className="text-3xl font-bold text-white">
            Dashboard Overview
          </h1>

          <p className="mt-2 text-slate-400">
            Live summary of devices, connectivity and alarms.
          </p>
        </div>

        <button
          type="button"
          onClick={refreshDashboard}
          disabled={isRefreshing}
          className="flex items-center justify-center gap-2 rounded-xl border border-slate-700 bg-slate-900 px-4 py-2.5 text-sm text-slate-300 transition hover:border-cyan-500/40 hover:text-cyan-300 disabled:opacity-50"
        >
          <RefreshCw
            size={17}
            className={
              isRefreshing
                ? 'animate-spin'
                : ''
            }
          />

          Refresh
        </button>
      </div>

      <div className="mt-8 grid gap-4 sm:grid-cols-2 xl:grid-cols-5">
        <SummaryCard
          title="Total Devices"
          value={summary.total_devices}
          icon={Boxes}
          color="cyan"
        />

        <SummaryCard
          title="Online Devices"
          value={summary.online_devices}
          icon={CircleCheck}
          color="green"
        />

        <SummaryCard
          title="Offline Devices"
          value={summary.offline_devices}
          icon={CircleX}
          color="red"
        />

        <SummaryCard
          title="Active Alarms"
          value={summary.active_alarms}
          icon={BellRing}
          color="amber"
        />

        <SummaryCard
          title="Critical Alarms"
          value={summary.critical_alarms}
          icon={AlertTriangle}
          color="red"
        />
      </div>

      <div className="mt-8 grid gap-6 xl:grid-cols-2">
        <article className="overflow-hidden rounded-2xl border border-slate-800 bg-slate-900/60">
          <div className="flex items-center justify-between border-b border-slate-800 px-6 py-5">
            <div>
              <h2 className="font-semibold text-white">
                Devices
              </h2>

              <p className="mt-1 text-sm text-slate-500">
                Current connectivity status
              </p>
            </div>

            <span className="rounded-lg bg-slate-800 px-3 py-1 text-sm text-slate-400">
              {devices.length} devices
            </span>
          </div>

          <div className="divide-y divide-slate-800">
            {devices.length === 0 && (
              <p className="p-6 text-center text-slate-500">
                No devices found.
              </p>
            )}

            {devices.map((device) => {
              const online =
                device.status?.toLowerCase()
                === 'online'

              return (
                <div
                  key={device.device_id}
                  className="flex items-center justify-between px-6 py-5"
                >
                  <div>
                    <p className="font-medium text-white">
                      {device.device_id}
                    </p>

                    <p className="mt-1 text-sm text-slate-500">
                      Firmware {device.firmware_version ?? 'Unknown'}
                    </p>
                  </div>

                  <span
                    className={[
                      'rounded-full px-3 py-1 text-xs font-medium',
                      online
                        ? 'bg-emerald-500/15 text-emerald-400'
                        : 'bg-red-500/15 text-red-400',
                    ].join(' ')}
                  >
                    {online ? 'Online' : 'Offline'}
                  </span>
                </div>
              )
            })}
          </div>
        </article>

        <article className="overflow-hidden rounded-2xl border border-slate-800 bg-slate-900/60">
          <div className="flex items-center justify-between border-b border-slate-800 px-6 py-5">
            <div>
              <h2 className="font-semibold text-white">
                Active Alarms
              </h2>

              <p className="mt-1 text-sm text-slate-500">
                Latest unresolved alarm states
              </p>
            </div>

            <span className="rounded-lg bg-amber-500/10 px-3 py-1 text-sm text-amber-400">
              {activeAlarms.length} active
            </span>
          </div>

          <div className="divide-y divide-slate-800">
            {activeAlarms.length === 0 && (
              <div className="p-8 text-center">
                <CircleCheck
                  className="mx-auto text-emerald-400"
                  size={30}
                />

                <p className="mt-3 text-slate-400">
                  No active alarms.
                </p>
              </div>
            )}

            {activeAlarms.map((alarm) => (
              <div
                key={`${alarm.device_id}-${alarm.record_id}`}
                className="px-6 py-5"
              >
                <div className="flex items-start justify-between gap-4">
                  <div>
                    <p className="font-medium text-white">
                      {alarm.alarm_code ?? 'Unknown Alarm'}
                    </p>

                    <p className="mt-1 text-sm text-slate-500">
                      {alarm.device_id}
                      {' • '}
                      {alarm.source_signal ?? 'Unknown signal'}
                    </p>
                  </div>

                  <span
                    className={[
                      'rounded-full px-3 py-1 text-xs font-medium',
                      severityClass(
                        alarm.severity,
                      ),
                    ].join(' ')}
                  >
                    {alarm.severity ?? 'Unknown'}
                  </span>
                </div>

                <p className="mt-3 text-xs uppercase tracking-wider text-amber-400">
                  {alarm.transition ?? 'Unknown state'}
                </p>
              </div>
            ))}
          </div>
        </article>
      </div>
    </section>
  )
}


export default DashboardPage