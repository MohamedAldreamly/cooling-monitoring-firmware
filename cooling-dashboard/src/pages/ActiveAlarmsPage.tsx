import { useQuery } from '@tanstack/react-query'
import {
  AlertTriangle,
  BellRing,
  CircleCheck,
  RefreshCw,
} from 'lucide-react'

import {
  getActiveAlarms,
} from '../api/dashboardApi'


function severityStyle(
  severity?: string,
) {
  switch (
    severity?.toUpperCase()
  ) {
    case 'CRITICAL':
      return {
        badge:
          'bg-red-500/15 text-red-400',
        border:
          'border-red-500/30',
        icon:
          'bg-red-500/15 text-red-400',
      }

    case 'HIGH':
      return {
        badge:
          'bg-orange-500/15 text-orange-400',
        border:
          'border-orange-500/30',
        icon:
          'bg-orange-500/15 text-orange-400',
      }

    case 'WARNING':
      return {
        badge:
          'bg-amber-500/15 text-amber-400',
        border:
          'border-amber-500/30',
        icon:
          'bg-amber-500/15 text-amber-400',
      }

    default:
      return {
        badge:
          'bg-slate-700 text-slate-300',
        border:
          'border-slate-700',
        icon:
          'bg-slate-800 text-slate-400',
      }
  }
}


function formatAlarmTime(
  receivedAt?: string,
  receivedAtMs?: number,
) {
  if (receivedAt) {
    const parsed =
      new Date(receivedAt)

    if (!Number.isNaN(
      parsed.getTime(),
    )) {
      return parsed.toLocaleString()
    }
  }

  if (receivedAtMs) {
    return new Date(
      receivedAtMs,
    ).toLocaleString()
  }

  return 'Time unavailable'
}


function ActiveAlarmsPage() {
  const alarmsQuery = useQuery({
    queryKey: ['active-alarms'],
    queryFn: getActiveAlarms,
    refetchInterval: 10000,
  })

  function refreshAlarms() {
    void alarmsQuery.refetch()
  }

  if (alarmsQuery.isLoading) {
    return (
      <div className="flex min-h-80 items-center justify-center">
        <RefreshCw
          className="animate-spin text-cyan-400"
          size={32}
        />
      </div>
    )
  }

  if (
    alarmsQuery.isError
    || !alarmsQuery.data
  ) {
    return (
      <div className="rounded-2xl border border-red-500/20 bg-red-500/5 p-8">
        <h2 className="text-xl font-semibold text-red-400">
          Unable to load active alarms
        </h2>

        <p className="mt-2 text-slate-400">
          Check the API connection and try again.
        </p>

        <button
          type="button"
          onClick={refreshAlarms}
          className="mt-5 rounded-xl bg-red-500 px-5 py-2.5 text-white"
        >
          Try again
        </button>
      </div>
    )
  }

  const {
    alarms,
    count,
    critical_count: criticalCount,
  } = alarmsQuery.data

  const highCount = alarms.filter(
    (alarm) =>
      alarm.severity?.toUpperCase()
      === 'HIGH',
  ).length

  return (
    <section>
      <div className="flex flex-col justify-between gap-4 md:flex-row md:items-center">
        <div>
          <h1 className="text-3xl font-bold text-white">
            Active Alarms
          </h1>

          <p className="mt-2 text-slate-400">
            Monitor unresolved alarm conditions across all devices.
          </p>
        </div>

        <button
          type="button"
          onClick={refreshAlarms}
          disabled={
            alarmsQuery.isFetching
          }
          className="flex items-center justify-center gap-2 rounded-xl border border-slate-700 bg-slate-900 px-4 py-2.5 text-sm text-slate-300 disabled:opacity-50"
        >
          <RefreshCw
            size={17}
            className={
              alarmsQuery.isFetching
                ? 'animate-spin'
                : ''
            }
          />

          Refresh
        </button>
      </div>

      <div className="mt-8 grid gap-4 sm:grid-cols-3">
        <div className="rounded-2xl border border-amber-500/20 bg-amber-500/5 p-5">
          <BellRing className="text-amber-400" />

          <p className="mt-4 text-sm text-slate-400">
            Active Alarms
          </p>

          <p className="mt-2 text-3xl font-bold text-white">
            {count}
          </p>
        </div>

        <div className="rounded-2xl border border-red-500/20 bg-red-500/5 p-5">
          <AlertTriangle className="text-red-400" />

          <p className="mt-4 text-sm text-slate-400">
            Critical
          </p>

          <p className="mt-2 text-3xl font-bold text-red-400">
            {criticalCount}
          </p>
        </div>

        <div className="rounded-2xl border border-orange-500/20 bg-orange-500/5 p-5">
          <AlertTriangle className="text-orange-400" />

          <p className="mt-4 text-sm text-slate-400">
            High Severity
          </p>

          <p className="mt-2 text-3xl font-bold text-orange-400">
            {highCount}
          </p>
        </div>
      </div>

      {alarms.length === 0 ? (
        <div className="mt-8 rounded-2xl border border-emerald-500/20 bg-emerald-500/5 p-12 text-center">
          <CircleCheck
            className="mx-auto text-emerald-400"
            size={44}
          />

          <h2 className="mt-4 text-xl font-semibold text-white">
            All systems normal
          </h2>

          <p className="mt-2 text-slate-400">
            There are no active alarms.
          </p>
        </div>
      ) : (
        <div className="mt-8 grid gap-5">
          {alarms.map((alarm) => {
            const styles =
              severityStyle(
                alarm.severity,
              )

            return (
              <article
                key={`${alarm.device_id}-${alarm.record_id}`}
                className={[
                  'rounded-2xl border bg-slate-900/60 p-6',
                  styles.border,
                ].join(' ')}
              >
                <div className="flex flex-col justify-between gap-5 lg:flex-row lg:items-start">
                  <div className="flex items-start gap-4">
                    <div
                      className={[
                        'flex size-12 shrink-0 items-center justify-center rounded-xl',
                        styles.icon,
                      ].join(' ')}
                    >
                      <AlertTriangle
                        size={24}
                      />
                    </div>

                    <div>
                      <div className="flex flex-wrap items-center gap-3">
                        <h2 className="font-semibold text-white">
                          {alarm.alarm_code
                            ?? 'Unknown Alarm'}
                        </h2>

                        <span
                          className={[
                            'rounded-full px-3 py-1 text-xs font-medium',
                            styles.badge,
                          ].join(' ')}
                        >
                          {alarm.severity
                            ?? 'Unknown'}
                        </span>

                        <span className="rounded-full bg-amber-500/10 px-3 py-1 text-xs font-medium text-amber-400">
                          {alarm.transition
                            ?? 'Unknown state'}
                        </span>
                      </div>

                      <p className="mt-3 text-sm text-slate-400">
                        Device:{' '}
                        <span className="text-slate-200">
                          {alarm.device_id}
                        </span>
                      </p>

                      <p className="mt-1 text-sm text-slate-400">
                        Source signal:{' '}
                        <span className="text-slate-200">
                          {alarm.source_signal
                            ?? 'Unknown'}
                        </span>
                      </p>
                    </div>
                  </div>

                  <div className="grid min-w-64 grid-cols-2 gap-4 rounded-xl bg-slate-950/60 p-4 text-sm">
                    <div>
                      <p className="text-slate-500">
                        Current value
                      </p>

                      <p className="mt-1 font-medium text-white">
                        {alarm.source_value
                          ?? 'Unavailable'}
                      </p>
                    </div>

                    <div>
                      <p className="text-slate-500">
                        Quality
                      </p>

                      <p className="mt-1 font-medium text-white">
                        {alarm.source_quality
                          ?? 'Unknown'}
                      </p>
                    </div>

                    <div>
                      <p className="text-slate-500">
                        Record
                      </p>

                      <p className="mt-1 font-medium text-white">
                        #{alarm.record_id}
                      </p>
                    </div>

                    <div>
                      <p className="text-slate-500">
                        Instance
                      </p>

                      <p className="mt-1 font-medium text-white">
                        {alarm.alarm_instance_id
                          ?? 'Unknown'}
                      </p>
                    </div>
                  </div>
                </div>

                <p className="mt-5 border-t border-slate-800 pt-4 text-xs text-slate-500">
                  {formatAlarmTime(
                    alarm.received_at,
                    alarm.received_at_ms,
                  )}
                </p>
              </article>
            )
          })}
        </div>
      )}
    </section>
  )
}


export default ActiveAlarmsPage