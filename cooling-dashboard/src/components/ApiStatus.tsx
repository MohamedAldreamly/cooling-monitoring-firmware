import { useQuery } from '@tanstack/react-query'

import {
  getDashboardSummary,
} from '../api/dashboardApi'


interface ApiStatusProps {
  showRegion?: boolean
}


function ApiStatus({
  showRegion = false,
}: ApiStatusProps) {
  const apiQuery = useQuery({
    queryKey: ['dashboard-summary'],
    queryFn: getDashboardSummary,
    refetchInterval: 15000,
    retry: 1,
  })

  const online =
    apiQuery.isSuccess

  const checking =
    apiQuery.isLoading
    || apiQuery.isFetching

  return (
    <div>
      <div
        className={[
          'flex items-center gap-2 text-sm',
          online
            ? 'text-emerald-400'
            : checking
              ? 'text-amber-400'
              : 'text-red-400',
        ].join(' ')}
      >
        <span
          className={[
            'size-2 rounded-full',
            online
              ? 'bg-emerald-400'
              : checking
                ? 'animate-pulse bg-amber-400'
                : 'bg-red-400',
          ].join(' ')}
        />

        {online
          ? 'API Online'
          : checking
            ? 'Checking API'
            : 'API Offline'}
      </div>

      {showRegion && (
        <p className="mt-2 text-xs text-slate-500">
          Region: us-east-1
        </p>
      )}
    </div>
  )
}


export default ApiStatus