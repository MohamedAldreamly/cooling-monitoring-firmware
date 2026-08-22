interface PlaceholderPageProps {
  title: string
  description: string
}


function PlaceholderPage({
  title,
  description,
}: PlaceholderPageProps) {
  return (
    <section>
      <h1 className="text-3xl font-bold text-white">
        {title}
      </h1>

      <p className="mt-2 text-slate-400">
        {description}
      </p>

      <div className="mt-8 rounded-2xl border border-dashed border-slate-700 bg-slate-900/50 p-12 text-center text-slate-500">
        This section will be implemented next.
      </div>
    </section>
  )
}


export default PlaceholderPage