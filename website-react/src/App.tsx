import { useRef } from "react";
import Navbar from "./components/Navbar";
import Hero from "./components/Hero";
import Features from "./components/Features";
import Screenshots from "./components/Screenshots";
import Download from "./components/Download";
import Footer from "./components/Footer";

export default function App() {
  const featuresRef = useRef<HTMLDivElement>(null);
  const screenshotsRef = useRef<HTMLDivElement>(null);
  const downloadRef = useRef<HTMLDivElement>(null);

  const scrollTo = (ref: React.RefObject<HTMLDivElement | null>) => {
    ref.current?.scrollIntoView({ behavior: "smooth" });
  };

  return (
    <div className="relative min-h-screen flex flex-col">
      {/* Background gradient blobs */}
      <div className="pointer-events-none fixed inset-0 overflow-hidden -z-10">
        <div className="absolute -top-[30%] -left-[10%] w-[60vw] h-[60vw] rounded-full bg-gradient-to-br from-indigo-700/30 via-purple-600/20 to-transparent blur-3xl animate-pulse-slow" />
        <div className="absolute top-[40%] -right-[15%] w-[50vw] h-[50vw] rounded-full bg-gradient-to-bl from-cyan-600/20 via-teal-500/10 to-transparent blur-3xl animate-pulse-slow delay-1000" />
      </div>

      <Navbar
        onFeaturesClick={() => scrollTo(featuresRef)}
        onScreenshotsClick={() => scrollTo(screenshotsRef)}
        onDownloadClick={() => scrollTo(downloadRef)}
      />

      <Hero onDownloadClick={() => scrollTo(downloadRef)} />
      <div ref={featuresRef}>
        <Features />
      </div>
      <div ref={screenshotsRef}>
        <Screenshots />
      </div>
      <div ref={downloadRef}>
        <Download />
      </div>
      <Footer />
    </div>
  );
}
