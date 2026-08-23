import { useQuery } from '@tanstack/react-query'
import {
  Activity,
  Cloud,
  Code2,
  Database,
  Eye,
  RefreshCw,
  Server,
  ShieldCheck,
} from 'lucide-react'

import { getDashboardSummary } from '../api/dashboardApi'


const apiBaseUrl =
  import.meta.env.VITE_API_BASE_URL


function SettingsPage() {
  const apiQuery = useQuery({
    queryKey: ['settings-api-check'],
    queryFn: getDashboardSummary,
    refetchInterval: 30000,
    retry: 1,
  })

  const apiOnline = apiQuery.isSuccess

  return (
    <section>
      <div className="flex flex-col justify-between gap-4 md:flex-row md:items-center">
        <div>
          <h1 className="text-3xl font-bold text-white">
            System Settings
          </h1>

          <p className="mt-2 text-slate-400">
            Read-only platform configuration and monitoring information.
          </p>
        </div>

        <div className="flex items-center gap-2 rounded-xl border border-emerald-500/20 bg-emerald-500/5 px-4 py-2.5 text-sm text-emerald-300">
          <ShieldCheck size={18} />
          Read-Only Mode
        </div>
      </div>

      <div className="mt-8 grid gap-5 xl:grid-cols-2">
        <article className="rounded-2xl border border-slate-800 bg-slate-900/60 p-6">
          <div className="flex items-center gap-3">
            <div className="flex size-11 items-center justify-center rounded-xl bg-cyan-500/10 text-cyan-400">
              <Cloud size={22} />
            </div>

            <div>
              <h2 className="font-semibold text-white">
                Cloud Connection
              </h2>
              <p className="text-sm text-slate-500">
                AWS backend connectivity
              </p>
            </div>
          </div>

          <dl className="mt-6 divide-y divide-slate-800">
            <div className="flex items-center justify-between gap-4 py-4">
              <dt className="text-sm text-slate-400">
                API Status
              </dt>
              <dd
                className={
                  apiOnline
                    ? 'text-sm font-medium text-emerald-400'
                    : apiQuery.isLoading
                      ? 'text-sm font-medium text-amber-400'
                      : 'text-sm font-medium text-red-400'
                }
              >
                {apiOnline
                  ? 'Online'
                  : apiQuery.isLoading
                    ? 'Checking'
                    : 'Unavailable'}
              </dd>
            </div>

            <div className="flex items-center justify-between gap-4 py-4">
              <dt className="text-sm text-slate-400">
                AWS Region
              </dt>
              <dd className="text-sm font-medium text-white">
                us-east-1
              </dd>
            </div>

            <div className="py-4">
              <dt className="text-sm text-slate-400">
                API Base URL
              </dt>
              <dd className="mt-2 break-all rounded-lg bg-slate-950 px-3 py-2 font-mono text-xs text-slate-300">
                {apiBaseUrl || 'Not configured'}
              </dd>
            </div>
          </dl>

          <button
            type="button"
            onClick={() => void apiQuery.refetch()}
            disabled={apiQuery.isFetching}
            className="mt-4 flex items-center gap-2 rounded-xl border border-slate-700 bg-slate-950 px-4 py-2.5 text-sm text-slate-300 transition hover:border-cyan-500/50 hover:text-white disabled:opacity-50"
          >
            <RefreshCw
              size={16}
              className={
                apiQuery.isFetching
                  ? 'animate-spin'
                  : ''
              }
            />
            Check Connection
          </button>
        </article>

        <article className="rounded-2xl border border-slate-800 bg-slate-900/60 p-6">
          <div className="flex items-center gap-3">
            <div className="flex size-11 items-center justify-center rounded-xl bg-violet-500/10 text-violet-400">
              <Activity size={22} />
            </div>

            <div>
              <h2 className="font-semibold text-white">
                Monitoring Profile
              </h2>
              <p className="text-sm text-slate-500">
                Dashboard refresh behavior
              </p>
            </div>
          </div>

          <dl className="mt-6 divide-y divide-slate-800">
            <div className="flex items-center justify-between gap-4 py-4">
              <dt className="text-sm text-slate-400">
                Telemetry Refresh
              </dt>
              <dd className="text-sm font-medium text-white">
                10 seconds
              </dd>
            </div>

            <div className="flex items-center justify-between gap-4 py-4">
              <dt className="text-sm text-slate-400">
                Device Refresh
              </dt>
              <dd className="text-sm font-medium text-white">
                30 seconds
              </dd>
            </div>

            <div className="flex items-center justify-between gap-4 py-4">
              <dt className="text-sm text-slate-400">
                Default Telemetry Range
              </dt>
              <dd className="text-sm font-medium text-white">
                1 hour
              </dd>
            </div>

            <div className="flex items-center justify-between gap-4 py-4">
              <dt className="text-sm text-slate-400">
                Maximum Samples
              </dt>
              <dd className="text-sm font-medium text-white">
                500 snapshots
              </dd>
            </div>
          </dl>
        </article>

        <article className="rounded-2xl border border-slate-800 bg-slate-900/60 p-6">
          <div className="flex items-center gap-3">
            <div className="flex size-11 items-center justify-center rounded-xl bg-blue-500/10 text-blue-400">
              <Server size={22} />
            </div>

            <div>
              <h2 className="font-semibold text-white">
                Data Services
              </h2>
              <p className="text-sm text-slate-500">
                Connected platform components
              </p>
            </div>
          </div>

          <div className="mt-6 grid gap-3 sm:grid-cols-2">
            {[
              ['AWS IoT Core', 'MQTT ingestion'],
              ['DynamoDB', 'Cloud data storage'],
              ['AWS Lambda', 'Backend processing'],
              ['API Gateway', 'Dashboard API'],
            ].map(([name, description]) => (
              <div
                key={name}
                className="rounded-xl border border-slate-800 bg-slate-950/70 p-4"
              >
                <div className="flex items-center gap-2 text-sm font-medium text-white">
                  <Database size={15} className="text-cyan-400" />
                  {name}
                </div>
                <p className="mt-2 text-xs text-slate-500">
                  {description}
                </p>
              </div>
            ))}
          </div>
        </article>

        <article className="rounded-2xl border border-slate-800 bg-slate-900/60 p-6">
          <div className="flex items-center gap-3">
            <div className="flex size-11 items-center justify-center rounded-xl bg-emerald-500/10 text-emerald-400">
              <Eye size={22} />
            </div>

            <div>
              <h2 className="font-semibold text-white">
                Access Policy
              </h2>
              <p className="text-sm text-slate-500">
                Dashboard operation mode
              </p>
            </div>
          </div>

          <div className="mt-6 rounded-xl border border-emerald-500/20 bg-emerald-500/5 p-5">
            <div className="flex items-center gap-2 font-medium text-emerald-300">
              <ShieldCheck size={18} />
              Monitoring Only
            </div>

            <p className="mt-3 text-sm leading-6 text-slate-400">
              This dashboard reads device status, telemetry, health, and alarm data. It does not modify device thresholds, Device Shadow settings, firmware configuration, or AWS resources.
            </p>
          </div>

          <div className="mt-5 flex items-center justify-between rounded-xl border border-slate-800 bg-slate-950/70 p-4">
            <div className="flex items-center gap-3">
              <Code2 size={19} className="text-slate-400" />
              <div>
                <p className="text-sm font-medium text-white">
                  Dashboard Version
                </p>
                <p className="text-xs text-slate-500">
                  Production monitoring build
                </p>
              </div>
            </div>

            <span className="rounded-lg bg-slate-800 px-3 py-1 text-xs font-medium text-slate-300">
              1.1.0
            </span>
          </div>
        </article>
      </div>
    </section>
  )
}


export default SettingsPage
